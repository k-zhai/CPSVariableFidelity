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

#ifndef MASTERNODE_H_
#define MASTERNODE_H_

#include "ExperimentControl.h"

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

#include <vector>
#include <string>
#include <chrono>

typedef std::chrono::system_clock Clock;

namespace inet {

class MasterNode : public cSimpleModule, public LifecycleUnsupported {

    private:
        simsignal_t directArrival;
        simsignal_t realTime;

        std::chrono::time_point<Clock> startClock;

    protected:
        vector<string> data;
        const vector<string> targets = {"DF1", "DF2"};

        const_simtime_t propagationDelay = 0.1;
        const_simtime_t frequency = 2;

        TcpSocket socket;
        simtime_t delay;
        simtime_t maxMsgDelay;

        long msgsRcvd;
        long msgsSent;
        long bytesRcvd;
        long bytesSent;

        std::map<int, ChunkQueue> socketQueue;

        simtime_t lastDirectMsgTime = 0;

    public:
        virtual void sendBack(cMessage *msg);
        virtual void sendOrSchedule(cMessage *msg, simtime_t delay);

        virtual void initialize(int stage) override;
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay() const override;

        void saveData(cMessage* msg);

        virtual void delayedMsgSend(cMessage* msg, int layer);
        virtual void finalMsgSend(cMessage* msg, const char* mod, int layer);
};

}

#endif /* MASTERNODE_H_ */
