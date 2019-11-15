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
#include "inet/common/INETDefs.h"

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"

#include <vector>
#include <string>
#include <queue>

using std::queue;

namespace inet {

class DFNode : public TcpAppBase, public LifecycleUnsupported {

    private:
        simsignal_t directArrival;
        simsignal_t tcpArrival;

    protected:
        vector<string> data;
        const vector<string> DF1targets = {"SN1", "SN2"};
        const vector<string> DF2targets = {"SN3", "SN4"};

        const_simtime_t propagationDelay = 0.1;
        const_simtime_t frequency = 2;

        TcpSocket socket;
        TcpSocket *socketToMaster = nullptr;
        simtime_t delay;
        simtime_t maxMsgDelay;

        // Server-side variables
        long msgsRcvd;
        long msgsSent;
        long bytesRcvd;
        long bytesSent;

        std::map<int, ChunkQueue> socketQueue;
        /* -------------------------------------------------------- */
        cMessage *timeoutMsg = nullptr;
        bool earlySend = false;
        int numRequestsToSend = 0;
        simtime_t startTime;
        simtime_t stopTime;

        simtime_t lastDirectMsgTime = 0;
        queue<simtime_t> tcpMsgTimes;

    public:
        DFNode();
        virtual ~DFNode();

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
        /* ----------------------------------------------------------------------- */
        virtual void sendRequest();
        virtual void rescheduleOrDeleteTimer(simtime_t d, short int msgKind);

        virtual void handleTimer(cMessage *msg) override;

        virtual void socketEstablished(TcpSocket *socket) override;
        virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
        virtual void socketClosed(TcpSocket *socket) override;
        virtual void socketFailure(TcpSocket *socket, int code) override;

        virtual void handleStartOperation(LifecycleOperation *operation) override;
        virtual void handleStopOperation(LifecycleOperation *operation) override;
        virtual void handleCrashOperation(LifecycleOperation *operation) override;

        virtual void close() override;

        void handleDirectMessage(cMessage *msg);
};

}

#endif /* DFNODE_H_ */
