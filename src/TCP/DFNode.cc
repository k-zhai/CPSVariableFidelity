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

#include "DFNode.h"

#include "inet/applications/common/SocketTag_m.h"
#include "inet/applications/tcpapp/GenericAppMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/TimeTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpCommand_m.h"

#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"

namespace inet {

#define MSGKIND_CONNECT    0
#define MSGKIND_SEND       1

Define_Module(DFNode);

DFNode::DFNode() {}

DFNode::~DFNode()
{
    cancelAndDelete(timeoutMsg);
}

void DFNode::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    TcpAppBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        delay = par("replyDelay");
        maxMsgDelay = 0;

        //statistics
        msgsRcvd = msgsSent = bytesRcvd = bytesSent = 0;

        WATCH(msgsRcvd);
        WATCH(msgsSent);
        WATCH(bytesRcvd);
        WATCH(bytesSent);
        /* --------------------------------------------------- */
        numRequestsToSend = 0;
        earlySend = false;
        WATCH(numRequestsToSend);
        WATCH(earlySend);

        startTime = par("startTime");
        stopTime = par("stopTime");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        timeoutMsg = new cMessage("timer");

        directArrival = registerSignal("directMsgArrived");
        tcpArrival = registerSignal("tcpPkArrived");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress[0] ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        socket.listen();

        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}

void DFNode::sendOrSchedule(cMessage *msg, simtime_t delay)
{
    if (delay == 0) {
        sendBack(msg);
    } else {
        scheduleAt(simTime() + delay, msg);
    }
}

void DFNode::sendBack(cMessage *msg)
{
    Packet *packet = dynamic_cast<Packet *>(msg);

    if (packet) {
        msgsSent++;
        bytesSent += packet->getByteLength();
        emit(packetSentSignal, packet);

        EV_INFO << "sending \"" << packet->getName() << "\" to TCP, " << packet->getByteLength() << " bytes\n";
    }
    else {
        EV_INFO << "sending \"" << msg->getName() << "\" to TCP\n";
    }

    auto& tags = getTags(msg);
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);
    send(msg, "socketOut");
}

void DFNode::handleMessage(cMessage *msg)
{
    if (ExperimentControl::getInstance().getSwitchStatus()) {
        if (msg->getKind() == msg_kind::TIMER || msg->getKind() == msg_kind::INIT_TIMER) {
            // Set time
            lastDirectMsgTime = simTime();

            // Schedule next TIMER call
            cMessage* tmMsg = new cMessage(nullptr, msg_kind::TIMER);
            scheduleAt(simTime() + frequency, tmMsg);

            // Schedule message to be finally sent after propagation delay
            EV_INFO << "delayedMsgSend " << simTime();
            delayedMsgSend(msg, ExperimentControl::getInstance().getState());
        } else if (msg->getKind() == msg_kind::APP_SELF_MSG) {
            EV_INFO << "finalMsgSend " << simTime();
            finalMsgSendRouter(msg, getParentModule()->getName());
        } else if (msg->getKind() == msg_kind::APP_MSG_RETURNED) {
            saveData(msg);
            emit(directArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(lastDirectMsgTime));
            ExperimentControl::getInstance().addDirectStats(lastDirectMsgTime, simTime());
        } else {
            handleDirectMessage(msg);
        }
        return;
    }

    if (msg->getKind() == msg_kind::RESTART_TCP) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(simTime(), timeoutMsg);
        delete msg;
        return;
    }

    if (msg->isSelfMessage()) {
        if (msg->getKind() == msg_kind::TIMER) {
            delete msg;
        } else if (msg->getKind() == MSGKIND_CONNECT || msg->getKind() == MSGKIND_SEND) {
            handleTimer(msg);
        } else {
            sendBack(msg);
        }
    }
    else if (msg->getKind() == TCP_I_PEER_CLOSED) {
        // we'll close too, but only after there's surely no message
        // pending to be sent back in this connection
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        delete msg;
        auto request = new Message("close", TCP_C_CLOSE);
        request->addTagIfAbsent<SocketReq>()->setSocketId(connId);
        sendOrSchedule(request, delay + maxMsgDelay);
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        if (connId == 23 || connId == 26 || connId == 37 || connId == 38) {
            if (msg->getKind() == TCP_I_URGENT_DATA) {
                socketDataArrived(socketToMaster, check_and_cast<Packet *>(msg), true);
            } else {
                socketDataArrived(socketToMaster, check_and_cast<Packet *>(msg), false);
            }
            return;
        }
        ChunkQueue &queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);
        emit(packetReceivedSignal, packet);

        bool doClose = false;
        while (const auto& appmsg = queue.pop<GenericAppMsg>(b(-1), Chunk::PF_ALLOW_NULLPTR)) {
            msgsRcvd++;
            bytesRcvd += B(appmsg->getChunkLength()).get();
            B requestedBytes = appmsg->getExpectedReplyLength();
            simtime_t msgDelay = appmsg->getReplyDelay();
            if (msgDelay > maxMsgDelay)
                maxMsgDelay = msgDelay;

            if (requestedBytes > B(0)) {
                Packet *outPacket = new Packet(msg->getName());
                outPacket->addTagIfAbsent<SocketReq>()->setSocketId(connId);
                outPacket->setKind(TCP_C_SEND);
                const auto& payload = makeShared<GenericAppMsg>();
                payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
                payload->setChunkLength(requestedBytes);
                payload->setExpectedReplyLength(B(0));
                payload->setReplyDelay(0);
                outPacket->insertAtBack(payload);
                sendOrSchedule(outPacket, delay + msgDelay);
            }
            if (appmsg->getServerClose()) {
                doClose = true;
                break;
            }
        }
        delete msg;

        if (doClose) {
            auto request = new Message("close", TCP_C_CLOSE);
            TcpCommand *cmd = new TcpCommand();
            request->addTagIfAbsent<SocketReq>()->setSocketId(connId);
            request->setControlInfo(cmd);
            sendOrSchedule(request, delay + maxMsgDelay);
        }
    } else if (msg->getKind() == TCP_I_AVAILABLE) {
        socket.processMessage(msg);
    } else {
        if (msg->getKind() == TCP_I_ESTABLISHED) {
            if (socketToMaster == nullptr) {
                socketToMaster = new TcpSocket(msg);
                socketToMaster->setOutputGate(gate("socketOut"));
                socketEstablished(socketToMaster);
            }
            delete msg;
        } else {
            // some indication -- ignore
            EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
            delete msg;
        }
    }
}

void DFNode::handleDirectMessage(cMessage *msg) {
    if (msg->getKind() == msg_kind::APP_MSG_SENT) {
        delete msg;
        msg = new cMessage(nullptr, msg_kind::APP_SELF_MSG_CLIENT);
        scheduleAt(simTime() + propagationDelay, msg);
    } else if (msg->getKind() == msg_kind::APP_SELF_MSG_CLIENT) {
        msg->setKind(msg_kind::APP_MSG_RETURNED);
        cModule *targetModule = getModuleByPath("TCPnetworksim.M.app[0]");
        sendDirect(msg, targetModule, "appIn");
    } else if (msg->getKind() == msg_kind::STOP_TCP) {
        socketToMaster->destroy();
        delete socketToMaster;
        socketToMaster = nullptr;
        cancelEvent(timeoutMsg);
        delete msg;
    } else if (msg->getKind() == msg_kind::RESTART_TCP) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(simTime(), timeoutMsg);
        delete msg;
    } else {
        delete msg;
    }
}

void DFNode::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void DFNode::finish()
{
    cancelAndDelete(timeoutMsg);
    delete socketToMaster;
    socketToMaster = nullptr;
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

void DFNode::saveData(cMessage* msg) {
    data.push_back("data");
    delete msg;
}

void DFNode::delayedMsgSend(cMessage* msg, int layer) {
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

void DFNode::finalMsgSend(cMessage* msg, const char* mod, int layer) {
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

void DFNode::finalMsgSendRouter(cMessage* msg, const char* currentMod) {
    if (!msg->isSelfMessage()) {
        error("Must be self message");
    }
    if (strcmp(currentMod, "DF1") == 0) {
        for (std::string s : DF1targets) {
            std::string targetPath("TCPnetworksim." + s + ".app[0]");
            finalMsgSend(msg, targetPath.c_str(), ExperimentControl::getInstance().getState());
        }
    } else if (strcmp(currentMod, "DF2") == 0) {
        for (std::string s : DF2targets) {
            std::string targetPath("TCPnetworksim." + s + ".app[0]");
            finalMsgSend(msg, targetPath.c_str(), ExperimentControl::getInstance().getState());
        }
    } else {
        error("Current module not valid");
    }
    delete msg;
}

/* ---------------------------------------------------------------------------------------------- */

void DFNode::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t now = simTime();
    simtime_t start = std::max(startTime, now);
    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
}

void DFNode::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (socketToMaster->getState() == TcpSocket::CONNECTED || socketToMaster->getState() == TcpSocket::CONNECTING || socketToMaster->getState() == TcpSocket::PEER_CLOSED)
        close();
}

void DFNode::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (operation->getRootModule() != getContainingNode(this))
        socketToMaster->destroy();
}

void DFNode::sendRequest()
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

void DFNode::handleTimer(cMessage *msg)
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

void DFNode::socketEstablished(TcpSocket *socket)
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

void DFNode::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
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

void DFNode::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
{
    TcpAppBase::socketDataArrived(socket, msg, urgent);

    if (!tcpMsgTimes.empty()) {
        if (SIMTIME_DBL(tcpMsgTimes.front()) < 10) {
            emit(tcpArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(tcpMsgTimes.front()));
            ExperimentControl::getInstance().addTcpStats(tcpMsgTimes.front(), simTime());
        }
        tcpMsgTimes.pop();
    }

    if (numRequestsToSend > 0 && !ExperimentControl::getInstance().getSwitchStatus()) {
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

void DFNode::close()
{
    TcpAppBase::close();
    cancelEvent(timeoutMsg);
}

void DFNode::socketClosed(TcpSocket *socket)
{
    TcpAppBase::socketClosed(socket);

    // start another session after a delay
    if (timeoutMsg && !ExperimentControl::getInstance().getSwitchStatus()) {
        simtime_t d = simTime() + par("idleInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

void DFNode::socketFailure(TcpSocket *socket, int code)
{
    TcpAppBase::socketFailure(socket, code);

    // reconnect after a delay
    if (timeoutMsg) {
        simtime_t d = simTime() + par("reconnectInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

}
