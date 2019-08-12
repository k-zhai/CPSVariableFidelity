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

namespace inet {

Define_Module(MasterNodeUDP);

void MasterNodeUDP::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // init statistics
        numEchoed = 0;
        WATCH(numEchoed);

        directArrival = registerSignal("directMsgArrived");
    }
}

void MasterNodeUDP::handleMessageWhenUp(cMessage *msg)
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
            for (std::string s : targets) {
                std::string targetPath("UDPnetworksim." + s + ".app[0]");
                finalMsgSend(msg, targetPath.c_str(), ExperimentControlUDP::getInstance().getState());
            }
            delete msg;
        } else if (msg->getKind() == msg_kind::APP_MSG_RETURNED) {
            saveData(msg);
            emit(directArrival, SIMTIME_DBL(simTime()) - SIMTIME_DBL(lastDirectMsgTime));
            ExperimentControlUDP::getInstance().addDirectStats(lastDirectMsgTime, simTime());
        } else {
            delete msg;
        }
    } else if (msg->isSelfMessage() && msg->getKind() == msg_kind::TIMER) {
        delete msg;
    } else {
        socket.processMessage(msg);
    }
}

void MasterNodeUDP::saveData(cMessage* msg) {
    data.push_back("data");
    delete msg;
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
        default:
            error("Invalid route");
            break;
    }
}

void MasterNodeUDP::socketDataArrived(UdpSocket *socket, Packet *pk)
{
    // determine its source address/port
    L3Address remoteAddress = pk->getTag<L3AddressInd>()->getSrcAddress();
    int srcPort = pk->getTag<L4PortInd>()->getSrcPort();
    pk->clearTags();
    pk->trim();

    // statistics
    numEchoed++;
    emit(packetSentSignal, pk);
    // send back
    socket->sendTo(pk, remoteAddress, srcPort);
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
