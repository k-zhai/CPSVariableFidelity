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

#include "SensorNodeUDP.h"

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

Define_Module(SensorNodeUDP);

SensorNodeUDP::SensorNodeUDP() {}

SensorNodeUDP::~SensorNodeUDP() {
    cancelAndDelete(selfMsg);
}

void SensorNodeUDP::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numSent = 0;
        numReceived = 0;
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

        udpArrival = registerSignal("udpPkArrived");

        ExperimentControlUDP::getInstance().incrementNumNodes();
    }
}

void SensorNodeUDP::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    ApplicationBase::finish();
}

void SensorNodeUDP::setSocketOptions()
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

L3Address SensorNodeUDP::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    if (destAddresses[k].isUnspecified() || destAddresses[k].isLinkLocal()) {
        L3AddressResolver().tryResolve(destAddressStr[k].c_str(), destAddresses[k]);
    }
    return destAddresses[k];
}

void SensorNodeUDP::sendPacket()
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

void SensorNodeUDP::processStart()
{
    socket.setOutputGate(gate("socketOut"));
    const char *localAddress = par("localAddress");
    socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
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

void SensorNodeUDP::processSend()
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

void SensorNodeUDP::processStop()
{
    socket.close();
}

void SensorNodeUDP::handleMessageWhenUp(cMessage *msg)
{
    if (ExperimentControlUDP::getInstance().getState() == 1) {
        if (msg->getKind() == msg_kind::TIMER || msg->getKind() == msg_kind::INIT_TIMER) {
            // Schedule next TIMER call
            cMessage* tmMsg = new cMessage("timer", msg_kind::TIMER);
            scheduleAt(simTime() + frequency, tmMsg);

            // Schedule message to be finally sent after propagation delay
            EV_INFO << "delayedMsgSend " << getParentModule()->getName();
            delayedMsgSend(msg, ExperimentControlUDP::getInstance().getState());
        } else if (msg->getKind() == msg_kind::APP_SELF_MSG) {
            EV_INFO << "finalMsgSend " << getParentModule()->getName();
            finalMsgSend(msg, routes.find(getParentModule()->getName())->second.c_str(), ExperimentControlUDP::getInstance().getState());
            delete msg;
        } else if (msg->getKind() >= (short)50) {
            // Do command
            delete msg;
        } else {
            delete msg;
        }
    } else if (msg->getKind() == msg_kind::STOP_UDP) {
        socket.destroy();
        if (selfMsg->isSelfMessage()) {
            cancelEvent(selfMsg);
        }
        delete msg;
    } else if (msg->getKind() == msg_kind::RESTART_UDP) {
        selfMsg = new cMessage("restart", START);
        scheduleAt(simTime(), selfMsg);
        delete msg;
    } else if (msg->isSelfMessage()) { // sending
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
    } else { // receiving
        socket.processMessage(msg);
    }
}

void SensorNodeUDP::delayedMsgSend(cMessage* msg, int layer) {
    switch (layer) {
        case 1:
            delete msg;
            msg = new cMessage(nullptr, msg_kind::APP_SELF_MSG);
            scheduleAt(simTime() + propagationDelay, msg);
            break;
        case 2:
            break;
        default:
            error("Invalid route");
            break;
    }
}

void SensorNodeUDP::finalMsgSend(cMessage* msg, const char* mod, int layer) {
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

const char* SensorNodeUDP::getDirectDestination(const char* currentMod) const {
    if (strcmp(currentMod, "SN1") == 0 || strcmp(currentMod, "SN2") == 0) {
        return "UDPnetworksim.DF1.app[0]";
    } else if (strcmp(currentMod, "SN3") == 0 || strcmp(currentMod, "SN4") == 0) {
        return "UDPnetworksim.DF2.app[0]";
    }
    return "";
}

void SensorNodeUDP::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    // process incoming packet, this is where it executes instruction sent down from MasterNode
    processPacket(packet);
}

void SensorNodeUDP::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void SensorNodeUDP::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void SensorNodeUDP::refreshDisplay()
{
    ApplicationBase::refreshDisplay();

    char buf[100];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void SensorNodeUDP::processPacket(Packet *pk)
{
    // get instructions based on msg_kind

    emit(packetReceivedSignal, pk);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
    delete pk;
    numReceived++;

    ExperimentControlUDP::getInstance().appendTotalPacketsLost((long)(numSent-numReceived) - packetsLost);
    if (packetsLost < numSent-numReceived) {
        packetsLost = numSent-numReceived;
    }

    // Handle packet loss by deleting entries that were sent but never processed on the other side.
    while (udpMsgTimes.size() > 1) {
        udpMsgTimes.pop();
    }

    if (!udpMsgTimes.empty()) {
        if (SIMTIME_DBL(udpMsgTimes.front()) < 20) { // TODO add calculation for outliers
            emit(udpArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(udpMsgTimes.front()));
            ExperimentControlUDP::getInstance().addUdpStats(udpMsgTimes.front(), simTime());
        }
        udpMsgTimes.pop();
    }
}

void SensorNodeUDP::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
    }
}

void SensorNodeUDP::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void SensorNodeUDP::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
}


}
