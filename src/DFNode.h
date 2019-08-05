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

#include "ExperimentControl.h"

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

#include <vector>
#include <string>

namespace inet {

class DFNode : public cSimpleModule, public LifecycleUnsupported {

    protected:
        vector<string> data;
        vector<string> DF1targets = {"SN1", "SN2"};
        vector<string> DF2targets = {"SN3", "SN4"};

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

        void finalMsgSendRouter(cMessage* msg, const char* currentMod);
};

}

#endif /* DFNODE_H_ */
