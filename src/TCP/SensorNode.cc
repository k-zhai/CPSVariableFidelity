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

#include "SensorNode.h"

#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/TimeTag_m.h"

namespace inet {

#define MSGKIND_CONNECT    0
#define MSGKIND_SEND       1

Define_Module(SensorNode);

SensorNode::SensorNode() {}

SensorNode::~SensorNode()
{
    cancelAndDelete(timeoutMsg);
}

void SensorNode::initialize(int stage)
{
    TcpAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        numRequestsToSend = 0;
        earlySend = false;    // TBD make it parameter
        WATCH(numRequestsToSend);
        WATCH(earlySend);

        startTime = par("startTime");
        stopTime = par("stopTime");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        timeoutMsg = new cMessage("timer");

        tcpArrival = registerSignal("tcpPkArrived");
    }
}

void SensorNode::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t now = simTime();
    simtime_t start = std::max(startTime, now);
    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
}

void SensorNode::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
        close();
}

void SensorNode::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (operation->getRootModule() != getContainingNode(this))
        socket.destroy();
}

void SensorNode::sendRequest()
{
    long requestLength = par("requestLength");
    long replyLength = par("replyLength");
    if (requestLength < 1)
        requestLength = 1;
    if (replyLength < 1)
        replyLength = 1;

    const auto& payload = makeShared<GenericAppMsg>();
    Packet *packet = new Packet("data");
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
    payload->setChunkLength(B(requestLength));
    payload->setExpectedReplyLength(B(replyLength));
    payload->setServerClose(false);
    packet->insertAtBack(payload);

    EV_INFO << "sending request with " << requestLength << " bytes, expected reply length " << replyLength << " bytes,"
            << "remaining " << numRequestsToSend - 1 << " request\n";

    sendPacket(packet);
}

void SensorNode::handleTimer(cMessage *msg)
{
    switch (msg->getKind()) {
        case MSGKIND_CONNECT:
            connect();    // active OPEN

            // significance of earlySend: if true, data will be sent already
            // in the ACK of SYN, otherwise only in a separate packet (but still
            // immediately)
            if (earlySend)
                sendRequest();
            break;

        case MSGKIND_SEND:
            sendRequest();
            numRequestsToSend--;
            // no scheduleAt(): next request will be sent when reply to this one
            // arrives (see socketDataArrived())
            break;

        default:
            throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
    }
}

void SensorNode::socketEstablished(TcpSocket *socket)
{
    TcpAppBase::socketEstablished(socket);

    // determine number of requests in this session
    numRequestsToSend = par("numRequestsPerSession");
    if (numRequestsToSend < 1)
        numRequestsToSend = 1;

    // perform first request if not already done (next one will be sent when reply arrives)
    if (!earlySend)
        sendRequest();

    numRequestsToSend--;
}

void SensorNode::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
    cancelEvent(timeoutMsg);

    if (stopTime < SIMTIME_ZERO || d < stopTime) {
        timeoutMsg->setKind(msgKind);
        scheduleAt(d, timeoutMsg);
    }
    else {
        delete timeoutMsg;
        timeoutMsg = nullptr;
    }
}

void SensorNode::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{

    TcpAppBase::socketDataArrived(socket, msg, urgent);

    if (!tcpMsgTimes.empty()) {
        if (SIMTIME_DBL(tcpMsgTimes.front()) < 10) {
            emit(tcpArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(tcpMsgTimes.front()));
            ExperimentControl::getInstance().addTcpStats(tcpMsgTimes.front(), simTime());
        }
        tcpMsgTimes.pop();
    }

    if (numRequestsToSend > 0 && !switchActive) {
        EV_INFO << "reply arrived\n";

        if (timeoutMsg) {
            simtime_t d = simTime() + par("thinkTime");
            rescheduleOrDeleteTimer(d, MSGKIND_SEND);
        }

        tcpMsgTimes.push(simTime());
    }
    else if (socket->getState() != TcpSocket::LOCALLY_CLOSED) {
        EV_INFO << "reply to last request arrived, closing session\n";
        close();
    }
}

void SensorNode::close()
{
    TcpAppBase::close();
    cancelEvent(timeoutMsg);
}

void SensorNode::socketClosed(TcpSocket *socket)
{
    TcpAppBase::socketClosed(socket);

    // start another session after a delay
    if (timeoutMsg && !switchActive) {
        simtime_t d = simTime() + par("idleInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

void SensorNode::socketFailure(TcpSocket *socket, int code)
{
    TcpAppBase::socketFailure(socket, code);

    // reconnect after a delay
    if (timeoutMsg) {
        simtime_t d = simTime() + par("reconnectInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

void SensorNode::handleMessage(cMessage *msg) {
    if (msg->getKind() == msg_kind::APP_MSG_SENT) {
        delete msg;
        msg = new cMessage(nullptr, msg_kind::APP_SELF_MSG);
        scheduleAt(simTime() + propagationDelay, msg);
    } else if (msg->getKind() == msg_kind::APP_SELF_MSG) {
        msg->setKind(msg_kind::APP_MSG_RETURNED);
        cModule *targetModule = getModuleByPath(getDirectDestination(getParentModule()->getName()));
        sendDirect(msg, targetModule, "appIn");
    } else if (msg->getKind() == msg_kind::STOP_TCP) {
        switchActive = true;
        // TODO: Check for packets still in progress
        if (!tcpMsgTimes.empty()) {
            scheduleAt(simTime() + 0.01, msg);
        } else {
            socket.destroy();
            cancelEvent(timeoutMsg);
            delete msg;
        }
    } else if (msg->getKind() == msg_kind::RESTART_TCP) {
        switchActive = false;
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(simTime(), timeoutMsg);
        delete msg;
    } else {
        OperationalBase::handleMessage(msg);
    }
}

const char* SensorNode::getDirectDestination(const char* currentMod) const {
    if (strcmp(currentMod, "SN1") == 0 || strcmp(currentMod, "SN2") == 0) {
        return "TCPnetworksim.DF1.app[0]";
    } else if (strcmp(currentMod, "SN3") == 0 || strcmp(currentMod, "SN4") == 0) {
        return "TCPnetworksim.DF2.app[0]";
    }
    return "";
}

}

