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

#ifndef EXPERIMENTCONTROL_H_
#define EXPERIMENTCONTROL_H_

#include <omnetpp.h>

using namespace omnetpp;

enum msgKind {
    START_MSG = 0,
    END_MSG = 1
};

class ExperimentControl : public cSimpleModule {

    private:
        static ExperimentControl* instance;
        bool state = par("switch");

    protected:
        int currentLayer;
        int newLayer;
        simtime_t startTime;
        simtime_t endTime;

        virtual void initialize() override;
        virtual void handleMessage(cMessage* msg) override;

    public:
        ExperimentControl() = default;
        ~ExperimentControl() = default;
        static ExperimentControl* getInstance();

        bool getState();
        void setState();
};

#endif /* EXPERIMENTCONTROL_H_ */
