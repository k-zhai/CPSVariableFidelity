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

namespace inet {

#define START_MSG   20
#define END_MSG     21

Define_Module(DFNode);

bool DFNode::switch_fidelity = false;
const_simtime_t DFNode::start_time = 5;
const_simtime_t DFNode::end_time = 10;

void DFNode::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        delay = par("replyDelay");
        maxMsgDelay = 0;

        //statistics
        msgsRcvd = msgsSent = bytesRcvd = bytesSent = 0;

        WATCH(msgsRcvd);
        WATCH(msgsSent);
        WATCH(bytesRcvd);
        WATCH(bytesSent);
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

    cMessage* startMsg = new cMessage("start_msg", START_MSG);
    cMessage* endMsg = new cMessage("end_msg", END_MSG);
    scheduleAt(start_time, startMsg);
    scheduleAt(end_time, endMsg);

    cMessage* timeout = new cMessage(nullptr, TIMER);
    scheduleAt(start_time, timeout);
}

void DFNode::sendOrSchedule(cMessage *msg, simtime_t delay)
{
    if (delay == 0) {
        cModule *current = getParentModule(); // TEST
        const char* name = getParentModule()->getFullName(); // TEST
        std::vector<const char *> gates = getGateNames(); // TEST
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
    // temporary
    if (msg->getKind() == START_MSG && msg->isSelfMessage()) {
        switch_fidelity = true;
        delete msg;
        return;
    } else if (msg->getKind() == END_MSG && msg->isSelfMessage()) {
        switch_fidelity = false;
        delete msg;
        return;
    }

    // temporary
    if (switch_fidelity) {
        if (msg->getKind() == TIMER) {
            // Schedule next TIMER call
            cMessage* tmMsg = new cMessage(nullptr, TIMER);
            scheduleAt(simTime() + frequency, tmMsg);

            // Schedule message to be finally sent after propagation delay
            EV_INFO << "delayedMsgSend " << simTime();
            delayedMsgSend(msg);
            return;
        } else if (msg->getKind() == APP_SELF_MSG) {
            EV_INFO << "finalMsgSend " << simTime();
            cModule *parent = getParentModule(); // TEST
            const char* parentname = getParentModule()->getFullName(); // TEST
            std::vector<const char *> gates = getGateNames(); // TEST
            cModule *current = this; // TEST
            const char* currentname = this->getFullName(); // TEST
            finalMsgSend(msg, "SN1");
            return;
        } else if (msg->getKind() == APP_MSG_RETURNED) {
            saveData(msg);
        }
    }

    if (msg->isSelfMessage()) {
        sendBack(msg);
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
    }
    else if (msg->getKind() == TCP_I_AVAILABLE)
        socket.processMessage(msg);
    else {
        // some indication -- ignore
        EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
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
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

void DFNode::saveData(cMessage* msg) {
    data.push_back("data");
    delete msg;
}

void DFNode::delayedMsgSend(cMessage* msg) {
    if (msg->isSelfMessage() && msg->getKind() == TIMER) {
        delete msg;
        msg = new cMessage(nullptr, APP_SELF_MSG);
        scheduleAt(simTime() + propagationDelay, msg);
    } else {
        error("Must be a self message with kind TIMER");
    }
}

void DFNode::finalMsgSend(cMessage* msg, const char* mod) {
    if (msg->isSelfMessage() && msg->getKind() == APP_SELF_MSG) {
        delete msg;
        msg = new cMessage(nullptr, APP_MSG_SENT);
        cModule *parent = getParentModule();
        cModule *parentparent = parent->getParentModule();
        cModule *parentparentchild = parentparent->getSubmodule(mod);
        cModule *parentparentchild2 = parentparent->getSubmodule("SN1");
        cModule *targetModule = getParentModule()->getParentModule()->getSubmodule("SN1")->getSubmodule("at");
        std::vector<const char *> targetgates = targetModule->getGateNames();
        sendDirect(msg, targetModule, "appIn");
    } else {
        error("Must be a self message with kind APP_SELF_MSG");
    }
}

}