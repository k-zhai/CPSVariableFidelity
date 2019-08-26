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

#include "DFNodeUDP.h"

#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

namespace inet {

Define_Module(DFNodeUDP);

DFNodeUDP::DFNodeUDP() {}

DFNodeUDP::~DFNodeUDP() {
    cancelAndDelete(selfMsg);
}

void DFNodeUDP::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // init statistics
        numEchoed = 0;
        numSent = 0;
        numReceived = 0;
        WATCH(numEchoed);
        WATCH(numSent);
        WATCH(numReceived);

        localPort = par("localPort");
        destPort = par("destPort");
        startTime = par("startTime");
        stopTime = par("stopTime");
        packetName = par("packetName");
        dontFragment = par("dontFragment");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        selfMsg = new cMessage("sendTimer");

        directArrival = registerSignal("directMsgArrived");
        udpArrival = registerSignal("udpPkArrived");
    }
}

void DFNodeUDP::handleMessageWhenUp(cMessage *msg)
{
    if (ExperimentControlUDP::getInstance().getSwitchStatus()) {
        if (msg->getKind() == msg_kind::TIMER || msg->getKind() == msg_kind::INIT_TIMER) {
            // Set time
            lastDirectMsgTime = simTime();

            // Schedule next TIMER call
            cMessage* tmMsg = new cMessage(nullptr, msg_kind::TIMER);
            scheduleAt(simTime() + frequency, tmMsg);

            // Schedule message to be finally sent after propagation delay
            EV_INFO << "delayedMsgSend " << simTime();
            delayedMsgSend(msg, ExperimentControlUDP::getInstance().getState());
        } else if (msg->getKind() == msg_kind::APP_SELF_MSG) {
            EV_INFO << "finalMsgSend " << simTime();
            finalMsgSendRouter(msg, getParentModule()->getName());
        } else if (msg->getKind() == msg_kind::APP_MSG_RETURNED) {
            saveData(msg);
            emit(directArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(lastDirectMsgTime));
            ExperimentControlUDP::getInstance().addDirectStats(lastDirectMsgTime, simTime());
        } else {
            handleDirectMessage(msg);
        }
    } else if (msg->getKind() == msg_kind::RESTART_UDP) {
        socket.setOutputGate(gate("socketOut"));
        int localPort = par("localPort");
        const char *localAddress = par("localAddress");
        socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
        socket.joinLocalMulticastGroups(mgl);
        socket.setCallback(this);

        selfMsg->setKind(START);
        scheduleAt(simTime(), selfMsg);
        delete msg;
    } else if (msg->isSelfMessage()) {
        if (msg->getKind() == msg_kind::TIMER) {
            delete msg;
        } else {
            ASSERT(msg == selfMsg);
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
    } else {
        socket.processMessage(msg);
    }
}

void DFNodeUDP::handleDirectMessage(cMessage *msg) {
    if (msg->getKind() == msg_kind::APP_MSG_SENT) {
        delete msg;
        msg = new cMessage(nullptr, msg_kind::APP_SELF_MSG_CLIENT);
        scheduleAt(simTime() + propagationDelay, msg);
    } else if (msg->getKind() == msg_kind::APP_SELF_MSG_CLIENT) {
        msg->setKind(msg_kind::APP_MSG_RETURNED);
        cModule* targetModule = getModuleByPath("UDPnetworksim.M.app[0]");
        sendDirect(msg, targetModule, "appIn");
    } else if (msg->getKind() == msg_kind::STOP_UDP) {
        socket.destroy();
        cancelEvent(selfMsg);
        delete msg;
    } else if (msg != selfMsg) {
        delete msg;
    }
}

void DFNodeUDP::saveData(cMessage* msg) {
    data.push_back("data");
    delete msg;
}

void DFNodeUDP::delayedMsgSend(cMessage* msg, int layer) {
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
        default:
            error("Invalid route");
            break;
    }
}

void DFNodeUDP::finalMsgSend(cMessage* msg, const char* mod, int layer) {
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
        default:
            error("Invalid route");
            break;
    }
}

void DFNodeUDP::finalMsgSendRouter(cMessage* msg, const char* currentMod) {
    if (!msg->isSelfMessage()) {
        error("Must be self message");
    }
    if (strcmp(currentMod, "DF1") == 0) {
        for (std::string s : DF1targets) {
            std::string targetPath("UDPnetworksim." + s + ".app[0]");
            finalMsgSend(msg, targetPath.c_str(), ExperimentControlUDP::getInstance().getState());
        }
    } else if (strcmp(currentMod, "DF2") == 0) {
        for (std::string s : DF2targets) {
            std::string targetPath("UDPnetworksim." + s + ".app[0]");
            finalMsgSend(msg, targetPath.c_str(), ExperimentControlUDP::getInstance().getState());
        }
    } else {
        error("Current module not valid");
    }
    delete msg;
}

void DFNodeUDP::socketDataArrived(UdpSocket *socket, Packet *pk)
{
    // determine its source address/port
    L3Address remoteAddress = pk->getTag<L3AddressInd>()->getSrcAddress();
    string check = remoteAddress.str();
    if (strcmp(remoteAddress.str().c_str(), "10.0.0.10") == 0 || strcmp(remoteAddress.str().c_str(), "10.0.0.22") == 0) {
        processPacket(pk);
    } else {
        int srcPort = pk->getTag<L4PortInd>()->getSrcPort();
        pk->clearTags();
        pk->trim();

        // statistics
        numEchoed++;
        emit(packetSentSignal, pk);
        // send back
        socket->sendTo(pk, remoteAddress, srcPort);
    }
}

void DFNodeUDP::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void DFNodeUDP::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void DFNodeUDP::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf1[40];
    char buf2[100];
    sprintf(buf1, "echoed: %d pks", numEchoed);
    sprintf(buf2, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
    getDisplayString().setTagArg("t", 0, buf1);
    getDisplayString().setTagArg("t", 0, buf2);
}

void DFNodeUDP::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    ApplicationBase::finish();
}

void DFNodeUDP::handleStartOperation(LifecycleOperation *operation)
{
    socket.setOutputGate(gate("socketOut"));
    int localPort = par("localPort");
    const char *localAddress = par("localAddress");
    socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
    MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
    socket.joinLocalMulticastGroups(mgl);
    socket.setCallback(this);

    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
    }
}

void DFNodeUDP::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void DFNodeUDP::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    if (operation->getRootModule() != getContainingNode(this))     // closes socket when the application crashed only
        socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
}

/* -------------------------------------------------------------------------------------------------------------------------------------------------- */

L3Address DFNodeUDP::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    if (destAddresses[k].isUnspecified() || destAddresses[k].isLinkLocal()) {
        L3AddressResolver().tryResolve(destAddressStr[k].c_str(), destAddresses[k]);
    }
    return destAddresses[k];
}

void DFNodeUDP::sendPacket()
{
    udpMsgTimes.push(simTime());

    std::ostringstream str;
    str << packetName << "-" << numSent;
    Packet *packet = new Packet(str.str().c_str());
    if(dontFragment)
        packet->addTagIfAbsent<FragmentationReq>()->setDontFragment(true);
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(par("messageLength")));
    payload->setSequenceNumber(numSent);
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
    packet->insertAtBack(payload);
    L3Address destAddr = chooseDestAddr();
    emit(packetSentSignal, packet);
    socket.sendTo(packet, destAddr, destPort);
    numSent++;
}

void DFNodeUDP::processPacket(Packet *pk)
{
    emit(packetReceivedSignal, pk);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
    delete pk;
    numReceived++;

    ExperimentControlUDP::getInstance().appendTotalPacketsLost((long)(numSent-numReceived) - packetsLost);
    if (packetsLost < numSent-numReceived) {
        packetsLost = numSent-numReceived;
    }

    if (!udpMsgTimes.empty()) {
        if (SIMTIME_DBL(udpMsgTimes.front()) < 20) {
            emit(udpArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(udpMsgTimes.front()));
            ExperimentControlUDP::getInstance().addUdpStats(udpMsgTimes.front(), simTime());
        }
        udpMsgTimes.pop();
    }
}

void DFNodeUDP::setSocketOptions() {
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

void DFNodeUDP::processStart()
{
    setSocketOptions();

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    while ((token = tokenizer.nextToken()) != nullptr) {
        destAddressStr.push_back(token);
        L3Address result;
        L3AddressResolver().tryResolve(token, result);
        if (result.isUnspecified())
            EV_ERROR << "cannot resolve destination address: " << token << endl;
        destAddresses.push_back(result);
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

void DFNodeUDP::processSend()
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

void DFNodeUDP::processStop()
{
    socket.close();
}

}

