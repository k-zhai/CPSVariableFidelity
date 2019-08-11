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

#include "ExperimentControl.h"

namespace inet {

Define_Module(ExperimentControl);

void ExperimentControl::initialize() {
    cSimpleModule::initialize();
    delete tcpMsgStats;
    delete directMsgStats;
    tcpMsgStats = getInstance().tcpMsgStats;
    directMsgStats = getInstance().directMsgStats;
    setState();
}

ExperimentControl::~ExperimentControl() {
    delete tcpMsgStats;
    delete directMsgStats;
    tcpMsgStats = nullptr;
    directMsgStats = nullptr;
}

void ExperimentControl::handleMessage(cMessage* msg) {
    if (msg->getKind() == msg_kind::START_MSG && msg->isSelfMessage()) {
        this->state = newLayer;
        getInstance().switchActive = true;
        delete msg;
    } else if (msg->getKind() == msg_kind::END_MSG && msg->isSelfMessage()) {
        this->state = currentLayer;
        getInstance().switchActive = false;
        delete msg;
        msg = new cMessage("restart_tcp", msg_kind::RESTART_TCP);
        sendToTargets(msg);
        delete msg;
    } else if (msg->getKind() == msg_kind::INIT_TIMER && msg->isSelfMessage()) {
        if (!getInstance().getSwitchStatus() && simTime() < end_time) {
            // to make sure timeout arrives after route has been switched
            scheduleAt(simTime() + 0.01, msg);
        } else {
            cMessage* stopMsg = new cMessage("stop_tcp", msg_kind::STOP_TCP);
            sendToTargets(stopMsg);
            sendToSources(msg);
            delete stopMsg;
            delete msg;
        }
    } else {
        delete msg;
    }
}

int ExperimentControl::getState() const {
    return getInstance().state;
}

bool ExperimentControl::getSwitchStatus() const {
    return getInstance().switchActive;
}

void ExperimentControl::setState() {
    if (currentLayer == newLayer) return;
    cMessage* startMsg = new cMessage("start_msg", msg_kind::START_MSG);
    cMessage* endMsg = new cMessage("end_msg", msg_kind::END_MSG);
    cMessage* timeout = new cMessage("timeout", msg_kind::INIT_TIMER);
    scheduleAt(start_time, startMsg);
    scheduleAt(end_time, endMsg);
    scheduleAt(start_time, timeout);
}

void ExperimentControl::sendToSources(cMessage *msg) {
    for (std::string s : sources) {
        std::string targetPath("TCPnetworksim." + s + ".app[0]");
        sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
    }
}

void ExperimentControl::sendToTargets(cMessage *msg) {
    for (std::string s : targets) {
        std::string targetPath("TCPnetworksim." + s + ".app[0]");
        sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
    }
}

void ExperimentControl::addTcpStats(simtime_t previousTime, simtime_t currentTime) {
    tcpMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControl::addDirectStats(simtime_t previousTime, simtime_t currentTime) {
    directMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControl::finish() {
    EV << "TCP time:       " << getInstance().tcpMsgStats->getMean() << endl;
    EV << "Direct time:    " << getInstance().directMsgStats->getMean() << endl;
}

}

