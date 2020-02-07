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
        upstream.clear();
        downstream.clear();
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

vector<string> ExperimentControlUDP::getMasterRoute(short kind) {
    if (getInstance().routingFromMaster.find(kind) == routingFromMaster.end()) {
        vector<string> none = {"none"};
        return none;
    }
    return getInstance().routingFromMaster.find(kind)->second;
}

void ExperimentControlUDP::handleMessage(cMessage* msg) {
    if (msg->getKind() == msg_kind::START_MSG && msg->isSelfMessage()) {
        getInstance().state = newLayer;
        getInstance().switchActive = true;
        delete msg;
    } else if (msg->getKind() == msg_kind::END_MSG && msg->isSelfMessage()) {
        getInstance().switchActive = false;
        delete msg;

        if (getInstance().state == 1) {
            msg = new cMessage("restart_udp", msg_kind::RESTART_UDP);
            sendToDownstream(msg);
            delete msg;
        } else if (getInstance().state == 2) {
//            cMessage* startMsg = new cMessage("start_L4", msg_kind_transport::L4_START);
//            sendToTargets(startMsg);
//            sendToSources(startMsg);
//            delete msg;
        }

        getInstance().state = currentLayer;

    } else if (msg->getKind() == msg_kind::INIT_TIMER && msg->isSelfMessage()) {
        if ((!getInstance().getSwitchStatus() && simTime() < end_time)) {
            // to make sure timeout arrives after route has been switched
            scheduleAt(simTime() + 0.01, msg);
        } else {
            if (getInstance().state == 1) {
                cMessage* stopMsg = new cMessage("stop_udp", msg_kind::STOP_UDP);
                sendToDownstream(stopMsg);
                sendToDownstream(msg);
                delete stopMsg;
                delete msg;
            } else if (getInstance().state == 2) {
//                cMessage* stopMsg = new cMessage("stop_L4", msg_kind_transport::L4_STOP);
//                sendToTargets(stopMsg);
//                sendToSources(stopMsg);
//                delete stopMsg;
//                delete msg;
            }
        }
    } else {
        delete msg;
    }
}

int ExperimentControlUDP::getNewLayer() const {
    return getInstance().newLayer;
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

void ExperimentControlUDP::sendToUpstream(cMessage *msg) {
    if (getInstance().state == 1) {
        for (std::string s : upstream) {
            std::string targetPath("UDPnetworksim." + s + ".app[0]");
            sendDirect(new cMessage("sending", msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
        }
    } else if (getInstance().state == 2) {
//        for (std::string s : sources) {
//            std::string targetPath("UDPnetworksim." + s + ".udp");
//            sendDirect(new cMessage("sending", msg->getKind()), getModuleByPath(targetPath.c_str()), "transportIn");
//        }
    }
}

void ExperimentControlUDP::sendToDownstream(cMessage *msg) {
    if (getInstance().state == 1) {
        for (std::string s : downstream) {
            std::string targetPath("UDPnetworksim." + s + ".app[0]");
            sendDirect(new cMessage("sending", msg->getKind()), getModuleByPath(targetPath.c_str()), "appIn");
        }
    } else if (getInstance().state == 2) {
//        for (std::string s : targets) {
//            std::string targetPath("UDPnetworksim." + s + ".udp");
//            sendDirect(new cMessage("sending", msg->getKind()), getModuleByPath(targetPath.c_str()), "transportIn");
//        }
    }
}

int ExperimentControlUDP::getNumNodes() const {
    return getInstance().numNodes;
}

int ExperimentControlUDP::getNumReady() const {
    return getInstance().numNodesReady;
}

void ExperimentControlUDP::incrementNumNodes() {
    ++getInstance().numNodes;
}

void ExperimentControlUDP::incrementNumReady() {
    ++getInstance().numNodesReady;
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
