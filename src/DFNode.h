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

#ifndef DFNODE_H_
#define DFNODE_H_

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

#include <vector>
#include <string>

namespace inet {

enum msgKind {
    APP_SELF_MSG = 11,
    APP_MSG_SENT = 12,
    APP_MSG_RETURNED = 13,
    TIMER = 14,
    RESTART_TCP = 15,
    STOP_TCP = 16
};

class DFNode : public cSimpleModule, public LifecycleUnsupported {

    protected:
        std::vector<std::string> data;

        const simtime_t propagationDelay = 0.01;
        const simtime_t frequency = 2;

        TcpSocket socket;
        simtime_t delay;
        simtime_t maxMsgDelay;

        long msgsRcvd;
        long msgsSent;
        long bytesRcvd;
        long bytesSent;

        std::map<int, ChunkQueue> socketQueue;

        // temporary, until ExperimentControl is fixed
        static bool switch_fidelity;
        static const_simtime_t start_time;
        static const_simtime_t end_time;

    public:
        virtual void sendBack(cMessage *msg);
        virtual void sendOrSchedule(cMessage *msg, simtime_t delay);

        virtual void initialize(int stage) override;
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay() const override;

        void saveData(cMessage* msg);

        virtual void delayedMsgSend(cMessage* msg);
        virtual void finalMsgSend(cMessage* msg, const char* mod);
};

}

#endif /* DFNODE_H_ */
