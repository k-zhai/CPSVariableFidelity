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

    if (!par("hasSwitch")) {
        sources.clear();
        targets.clear();
    }

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
        getInstance().state = newLayer;
        getInstance().switchActive = true;
        delete msg;
    } else if (msg->getKind() == msg_kind::END_MSG && msg->isSelfMessage()) {
        getInstance().switchActive = false;
        delete msg;

        if (getInstance().state == 1) {
            msg = new cMessage("restart_tcp", msg_kind::RESTART_TCP);
            sendToTargets(msg);
            delete msg;
        } else if (getInstance().state == 2) {
            cMessage* startMsg = new cMessage("start_L4", msg_kind_transport::L4_START);
            sendToTargets(startMsg);
            delete msg;
        }

        getInstance().state = currentLayer;

    } else if (msg->getKind() == msg_kind::INIT_TIMER && msg->isSelfMessage()) {
        if (!getInstance().getSwitchStatus() && simTime() < end_time) {
            // to make sure timeout arrives after route has been switched
            scheduleAt(simTime() + 0.01, msg);
        } else {
            if (getInstance().state == 1) {
                cMessage* stopMsg = new cMessage("stop_tcp", msg_kind::STOP_TCP);
                sendToTargets(stopMsg);
                sendToSources(msg);
                delete stopMsg;
                delete msg;
            } else if (getInstance().state == 2) {
                cMessage* stopMsg = new cMessage("stop_L4", msg_kind_transport::L4_STOP);
                sendToTargets(stopMsg);
                delete stopMsg;
                delete msg;
            }
        }
    } else {
        delete msg;
    }
}

int ExperimentControl::getNewLayer() const {
    return getInstance().newLayer;
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
    if (getInstance().state == 1) {
        for (std::string s : sources) {
            std::string targetPath("TCPnetworksim." + s + ".app[0]");
            sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
        }
    } else if (getInstance().state == 2) {
        for (std::string s : sources) {
            std::string targetPath("TCPnetworksim." + s + ".tcp");
            sendDirect(new cMessage("to source", msg->getKind()), getModuleByPath(targetPath.c_str()), "transportIn");
        }
    }
}

void ExperimentControl::sendToTargets(cMessage *msg) {
    if (getInstance().state == 1) {
        for (std::string s : targets) {
            std::string targetPath("TCPnetworksim." + s + ".app[0]");
            sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
        }
    } else if (getInstance().state == 2) {
        for (std::string s : targets) {
            std::string targetPath("TCPnetworksim." + s + ".tcp");
            sendDirect(new cMessage("to target", msg->getKind()), getModuleByPath(targetPath.c_str()), "transportIn");
        }
    }
}

void ExperimentControl::addTcpStats(simtime_t previousTime, simtime_t currentTime) {
    tcpMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControl::addDirectStats(simtime_t previousTime, simtime_t currentTime) {
    directMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControl::finish() {
    EV << "TCP time:" << endl;
    EV << "     Mean: " << getInstance().tcpMsgStats->getMean() << endl;
    EV << "     Min:  " << getInstance().tcpMsgStats->getMin() << endl;
    EV << "     Max:  " << getInstance().tcpMsgStats->getMax() << endl;
    EV << "Direct time:" << endl;
    EV << "     Mean: " << getInstance().directMsgStats->getMean() << endl;
    EV << "     Min:  " << getInstance().directMsgStats->getMin() << endl;
    EV << "     Max:  " << getInstance().directMsgStats->getMax() << endl;
}

}

