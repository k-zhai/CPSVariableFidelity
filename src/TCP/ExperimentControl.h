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

#include <vector>
#include <string>
#include <omnetpp.h>

using namespace omnetpp;
using std::string;
using std::vector;

namespace inet {

enum msg_kind : short int {
    APP_SELF_MSG = 11,
    APP_SELF_MSG_CLIENT = 12,
    APP_MSG_SENT = 13,
    APP_MSG_RETURNED = 14,
    INIT_TIMER = 15,
    TIMER = 16,
    RESTART_TCP = 17,
    STOP_TCP = 18,
    START_MSG = 20,
    END_MSG = 21
};

class ExperimentControl : public cSimpleModule {

    private:
        short int state = 1;
        bool switchActive = false;

    protected:
        const int currentLayer = 7;
        const int newLayer = 1;
        const_simtime_t start_time = 50;
        const_simtime_t end_time = 75;

        const vector<string> sources = {"DF1", "DF2", "M"};
        const vector<string> targets = {"SN1", "SN2", "SN3", "SN4", "DF1", "DF2"};

        virtual void initialize() override;
        virtual void handleMessage(cMessage* msg) override;

    public:
        static ExperimentControl instance;

        ExperimentControl() = default;
        virtual ~ExperimentControl();

        static ExperimentControl& getInstance() {
            static ExperimentControl instance;
            return instance;
        }

        cHistogram *tcpMsgStats = new cHistogram("tcpMsgStats");
        cHistogram *directMsgStats = new cHistogram("directMsgStats");

        int getState() const;
        bool getSwitchStatus() const;
        void setState();

        void sendToSources(cMessage *msg);
        void sendToTargets(cMessage *msg);

        void addTcpStats(simtime_t previousTime, simtime_t currentTime);
        void addDirectStats(simtime_t previousTime, simtime_t currentTime);

        virtual void finish() override;
};

}

#endif /* EXPERIMENTCONTROL_H_ */
