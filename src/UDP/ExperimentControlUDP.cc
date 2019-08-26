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

#include "ExperimentControlUDP.h"

namespace inet {

Define_Module(ExperimentControlUDP);

void ExperimentControlUDP::initialize() {
    cSimpleModule::initialize();

    if (!par("hasSwitch")) {
        sources.clear();
        targets.clear();
    }

    delete udpMsgStats;
    delete directMsgStats;
    udpMsgStats = getInstance().udpMsgStats;
    directMsgStats = getInstance().directMsgStats;
    setState();
}

ExperimentControlUDP::~ExperimentControlUDP() {
    delete udpMsgStats;
    delete directMsgStats;
    udpMsgStats = nullptr;
    directMsgStats = nullptr;
}

void ExperimentControlUDP::handleMessage(cMessage* msg) {
    if (msg->getKind() == msg_kind::START_MSG && msg->isSelfMessage()) {
        this->state = newLayer;
        getInstance().switchActive = true;
        delete msg;
    } else if (msg->getKind() == msg_kind::END_MSG && msg->isSelfMessage()) {
        this->state = currentLayer;
        getInstance().switchActive = false;
        delete msg;
        msg = new cMessage("restart_udp", msg_kind::RESTART_UDP);
        sendToTargets(msg);
        delete msg;
    } else if (msg->getKind() == msg_kind::INIT_TIMER && msg->isSelfMessage()) {
        if (!getInstance().getSwitchStatus() && simTime() < end_time) {
            // to make sure timeout arrives after route has been switched
            scheduleAt(simTime() + 0.01, msg);
        } else {
            cMessage* stopMsg = new cMessage("stop_udp", msg_kind::STOP_UDP);
            sendToTargets(stopMsg);
            sendToSources(msg);
            delete stopMsg;
            delete msg;
        }
    } else {
        delete msg;
    }
}

int ExperimentControlUDP::getState() const {
    return getInstance().state;
}

bool ExperimentControlUDP::getSwitchStatus() const {
    return getInstance().switchActive;
}

void ExperimentControlUDP::setState() {
    if (currentLayer == newLayer) return;
    cMessage* startMsg = new cMessage("start_msg", msg_kind::START_MSG);
    cMessage* endMsg = new cMessage("end_msg", msg_kind::END_MSG);
    cMessage* timeout = new cMessage("timeout", msg_kind::INIT_TIMER);
    scheduleAt(start_time, startMsg);
    scheduleAt(end_time, endMsg);
    scheduleAt(start_time, timeout);
}

void ExperimentControlUDP::sendToSources(cMessage *msg) {
    for (std::string s : sources) {
        std::string targetPath("UDPnetworksim." + s + ".app[0]");
        sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
    }
}

void ExperimentControlUDP::sendToTargets(cMessage *msg) {
    for (std::string s : targets) {
        std::string targetPath("UDPnetworksim." + s + ".app[0]");
        sendDirect(new cMessage(nullptr, msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
    }
}

void ExperimentControlUDP::addUdpStats(simtime_t previousTime, simtime_t currentTime) {
    udpMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControlUDP::addDirectStats(simtime_t previousTime, simtime_t currentTime) {
    directMsgStats->collect(SIMTIME_DBL(currentTime) - SIMTIME_DBL(previousTime));
}

void ExperimentControlUDP::appendTotalPacketsLost(long packets) {
    totalPacketsLost += packets;
}

long ExperimentControlUDP::getTotalPacketsLost() const {
    return totalPacketsLost;
}

void ExperimentControlUDP::finish() {
    EV << "UDP time:" << endl;
    EV << "     Mean: " << getInstance().udpMsgStats->getMean() << endl;
    EV << "     Min:  " << getInstance().udpMsgStats->getMin() << endl;
    EV << "     Max:  " << getInstance().udpMsgStats->getMax() << endl;
    EV << "Direct time:" << endl;
    EV << "     Mean: " << getInstance().directMsgStats->getMean() << endl;
    EV << "     Min:  " << getInstance().directMsgStats->getMin() << endl;
    EV << "     Max:  " << getInstance().directMsgStats->getMax() << endl;
}

}
