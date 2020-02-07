//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "MasterNodeUDP.h"

#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

namespace inet {

Define_Module(MasterNodeUDP);

void MasterNodeUDP::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        directArrival = registerSignal("directMsgArrived");
        realTime = registerSignal("recordWallTime");
        pkLossCount = registerSignal("totalPacketsLost");

        startClock = clock();

        destPort = par("destPort");

        startTime = par("startTime");
        stopTime = par("stopTime");
        packetName = par("packetName");

        cMessage *wallTime = new cMessage("record_wall_time", msg_kind::RECORD_TIME);
        scheduleAt(simTime(), wallTime);

        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        selfMsg = new cMessage("timer");
    }
}

void MasterNodeUDP::setSocketOptions()
{
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);

    int typeOfService = par("typeOfService");
    if (typeOfService != -1)
        socket.setTypeOfService(typeOfService);

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0]) {
        IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie)
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    }

    bool receiveBroadcast = par("receiveBroadcast");
    if (receiveBroadcast)
        socket.setBroadcast(true);

    bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
    if (joinLocalMulticastGroups) {
        MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
        socket.joinLocalMulticastGroups(mgl);
    }
    socket.setCallback(this);
}

L3Address MasterNodeUDP::chooseDestAddr(short msg_kind)
{
    // TODO: get address associated with msg_kind

    string destination = ExperimentControlUDP::getInstance().getMasterRoute(msg_kind)[0];

    cStringTokenizer tokenizer(destination.c_str());
    const char *token;

    L3Address result;
    while ((token = tokenizer.nextToken()) != nullptr) {
        L3AddressResolver().tryResolve(token, result);
    }

    return result;
}

void MasterNodeUDP::sendPacket()
{
    std::ostringstream str;
    short int kind = master_msg_kinds[intrand(master_msg_kinds.size()-1)];
    Packet *packet = new Packet(str.str().c_str(), kind);
    if(dontFragment)
        packet->addTagIfAbsent<FragmentationReq>()->setDontFragment(true);
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
    packet->insertAtBack(payload);
    L3Address destAddr = chooseDestAddr(kind);
    emit(packetSentSignal, packet);

    ExperimentControlUDP::getInstance().IDmap[packet->getId()] = kind;

    socket.sendTo(packet, destAddr, destPort);
}

void MasterNodeUDP::processStart()
{
    setSocketOptions();

    for (int i=0; i<(int)(destAddrs.size()); ++i) {
        cStringTokenizer tokenizer(destAddrs[i].c_str());
        const char *token;

        while ((token = tokenizer.nextToken()) != nullptr) {
            destAddressStr.push_back(token);
            L3Address result;
            L3AddressResolver().tryResolve(token, result);
            if (result.isUnspecified())
                EV_ERROR << "cannot resolve destination address: " << token << endl;
            destAddresses.push_back(result);
        }
    }

    if (!destAddresses.empty()) {
        selfMsg->setKind(SEND);
        processSend();
    }
    else {
        if (stopTime >= SIMTIME_ZERO) {
            selfMsg->setKind(STOP);
            scheduleAt(stopTime, selfMsg);
        }
    }
}

void MasterNodeUDP::processSend()
{
    sendPacket();
    simtime_t d = simTime() + par("sendInterval");
    if (stopTime < SIMTIME_ZERO || d < stopTime) {
        selfMsg->setKind(SEND);
        scheduleAt(d, selfMsg);
    }
    else {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
    }
}

void MasterNodeUDP::processStop()
{
    socket.close();
}

void MasterNodeUDP::handleMessageWhenUp(cMessage *msg)
{

    if (msg->getKind() == msg_kind::RECORD_TIME) {
        emit(realTime, (clock() - startClock) / (double) CLOCKS_PER_SEC);
        emit(pkLossCount, ExperimentControlUDP::getInstance().getTotalPacketsLost());
        scheduleAt(simTime() + 1, msg);
        return;
    }

    if (msg->isSelfMessage()) {
        ASSERT(msg == selfMsg);
        if (ExperimentControlUDP::getInstance().getSwitchStatus() && ExperimentControlUDP::getInstance().getState() == 1) {
            short int kind = master_msg_kinds[intrand(master_msg_kinds.size()-1)];
            cMessage *toSN = new cMessage("to SN", kind);
            string destination = "UDPnetworksim." + ExperimentControlUDP::getInstance().getMasterRoute(kind)[0] + ".app[0]";
            cModule *targetModule = getModuleByPath(destination.c_str());
            sendDirect(toSN, targetModule, "appIn");
            simtime_t d = simTime() + par("sendInterval");
            if (stopTime < SIMTIME_ZERO || d < stopTime) {
                selfMsg->setKind(SEND);
                scheduleAt(d, selfMsg);
            } else {
                selfMsg->setKind(STOP);
                scheduleAt(stopTime, selfMsg);
            }
        } else {
            switch (selfMsg->getKind()) {
                case START:
                    processStart();
                    break;

                case SEND:
                    processSend();
                    break;

                case STOP:
                    processStop();
                    break;

                default:
                    throw cRuntimeError("Invalid kind %d in self message", (int)selfMsg->getKind());
            }
        }
    } else if (msg->getKind() == msg_kind::APP_MSG_SENT) {
        // From DFNode application layer
//        Packet *packet = check_and_cast<Packet *>(msg);
//        saveData(packet);
        delete msg;
    } else {
        // From DFNode other layers
        socket.processMessage(msg);
    }
}

void MasterNodeUDP::saveData(Packet* pk) {
    data.push(*pk);
    delete pk;
}

void MasterNodeUDP::delayedMsgSend(cMessage* msg, int layer) {
    switch (layer) {
        case 1:
            if ((msg->isSelfMessage() && msg->getKind() == msg_kind::TIMER) || msg->getKind() == msg_kind::INIT_TIMER) {
                delete msg;
                msg = new cMessage(nullptr, msg_kind::APP_SELF_MSG);
                scheduleAt(simTime() + propagationDelay, msg);
            } else {
                error("Must be a self message with kind TIMER or message with kind INIT_TIMER");
            }
            break;
        case 2:
            break;
        default:
            error("Invalid route");
            break;
    }
}

void MasterNodeUDP::finalMsgSend(cMessage* msg, const char* mod, int layer) {
    switch (layer) {
        case 1:
            if (msg->getKind() == msg_kind::APP_SELF_MSG) {
                cMessage *tmp = new cMessage(nullptr, msg_kind::APP_MSG_SENT);
                cModule *targetModule = getModuleByPath(mod);
                sendDirect(tmp, targetModule, "appIn");
            } else {
                error("Must be a self message with kind APP_SELF_MSG");
            }
            break;
        case 2:
            break;
        default:
            error("Invalid route");
            break;
    }
}

void MasterNodeUDP::socketDataArrived(UdpSocket *socket, Packet *pk)
{
//    // determine its source address/port
//    L3Address remoteAddress = pk->getTag<L3AddressInd>()->getSrcAddress();
//    int srcPort = pk->getTag<L4PortInd>()->getSrcPort();
//    pk->clearTags();
//    pk->trim();
//
//    // statistics
//    numEchoed++;
//    emit(packetSentSignal, pk);
//    // send back
//    socket->sendTo(pk, remoteAddress, srcPort);

    saveData(pk);
}

void MasterNodeUDP::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void MasterNodeUDP::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void MasterNodeUDP::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf[40];
    sprintf(buf, "echoed: %d pks", numEchoed);
    getDisplayString().setTagArg("t", 0, buf);
}

void MasterNodeUDP::finish()
{
    ApplicationBase::finish();
}

void MasterNodeUDP::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
    }

    socket.setOutputGate(gate("socketOut"));
    int localPort = par("localPort");
    socket.bind(localPort);
    MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
    socket.joinLocalMulticastGroups(mgl);
    socket.setCallback(this);
}

void MasterNodeUDP::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void MasterNodeUDP::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this))     // closes socket when the application crashed only
        socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
}

}
