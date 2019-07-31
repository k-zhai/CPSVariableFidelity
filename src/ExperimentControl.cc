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

using omnetpp::cMessage;

Define_Module(ExperimentControl);

ExperimentControl* ExperimentControl::instance = nullptr;

void ExperimentControl::initialize() {
}

void ExperimentControl::handleMessage(cMessage* msg) {
    if (msg->getKind() == START_MSG && msg->isSelfMessage()) {
        this->state = true;
    } else if (msg->getKind() == END_MSG && msg->isSelfMessage()) {
        this->state = false;
    }
    delete msg;
}

ExperimentControl* ExperimentControl::getInstance() {
    if (!instance) {
        instance = new ExperimentControl;
    }
    return instance;
}

bool ExperimentControl::getState() {
    return this->state;
}

void ExperimentControl::setState() {
    if (currentLayer == newLayer) return;
    cMessage* startMsg = new cMessage("startMsg", START_MSG);
    cMessage* endMsg = new cMessage("endMsg", END_MSG);
    scheduleAt(startTime, startMsg);
    scheduleAt(endTime, endMsg);
}
