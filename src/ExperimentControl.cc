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

void ExperimentControl::initialize(int stage) {
    cSimpleModule::initialize(stage);
}

void ExperimentControl::handleMessage(cMessage* msg) {
    if (msg->getKind() == START_MSG && msg->isSelfMessage()) {
        this->state = newLayer;
        switchActive = true;
        delete msg;
        msg = new cMessage("stop_tcp", STOP_TCP);
        sendToTargets(msg);
    } else if (msg->getKind() == END_MSG && msg->isSelfMessage()) {
        this->state = currentLayer;
        switchActive = false;
        delete msg;
        msg = new cMessage("restart_tcp", RESTART_TCP);
        sendToTargets(msg);
    } else if (msg->getKind() == TIMER && msg->isSelfMessage()) {
        if (!switchActive && simTime() < end_time) {
            // to make sure timeout arrives after route has been switched
            scheduleAt(simTime() + 0.01, msg);
            return;
        }
        sendToSources(msg);
    } else {
        delete msg;
    }
}

ExperimentControl* ExperimentControl::getInstance() {
    if (!instance) {
        instance = new ExperimentControl;
    }
    return instance;
}

int ExperimentControl::getState() {
    return this->state;
}

bool ExperimentControl::getSwitchStatus() {
    return switchActive;
}

void ExperimentControl::setState() {
    if (currentLayer == newLayer) return;
    cMessage* startMsg = new cMessage("start_msg", START_MSG);
    cMessage* endMsg = new cMessage("end_msg", END_MSG);
    cMessage* timeout = new cMessage("timeout", TIMER);
    scheduleAt(start_time, startMsg);
    scheduleAt(end_time, endMsg);
    scheduleAt(start_time, timeout);
}

void ExperimentControl::sendToSources(cMessage *msg) {
    for (std::string s : sources) {
        std::string targetPath("networksim2." + s + "app[0]");
        sendDirect(msg, getModuleByPath(targetPath.c_str()), "appIn");
    }
}

void ExperimentControl::sendToTargets(cMessage *msg) {
    for (std::string s : targets) {
        std::string targetPath("networksim2." + s + "app[0]");
        sendDirect(msg, getModuleByPath(targetPath.c_str()), "appIn");
    }
}

}

