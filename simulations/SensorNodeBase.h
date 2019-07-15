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

#ifndef SENSORNODEBASE_H_
#define SENSORNODEBASE_H_

#include "inet/common/INETDefs.h"

#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

namespace inet {

enum msgKind {
    APP_SELF_MSG = 11,
    APP_MSG_SENT = 12,
    APP_MSG_RETURNED = 13,
    TIMER = 14
};

class SensorNodeBase : public cSimpleModule, public TcpAppBase, public LifecycleUnsupported {

    protected:

        TcpSocket socket;
        simtime_t delay;
        simtime_t maxMsgDelay;

        simtime_t propagationDelay = 0.01s; // placeholder
        simtime_t frequency = 2s; // placeholder

        long msgsRcvd;
        long msgsSent;
        long bytesRcvd;
        long bytesSent;

        std::map<int, ChunkQueue> socketQueue;

        cMessage *timeoutMsg = nullptr;
        bool earlySend = false;    // if true, don't wait with sendRequest() until established()
        int numRequestsToSend = 0;    // requests to send in this session
        simtime_t startTime;
        simtime_t stopTime;

        // temporary, until ExperimentControl is fixed
        static bool switch_fidelity = false;
        static simtime_t start_time = 10s;
        static simtime_t end_time = 30s;


    protected:

        SensorNodeBase();
        ~SensorNodeBase();

        void sendBack(cMessage *msg);
        void sendOrSchedule(cmessage *msg, simtime_t delay);

        void initialize(int stage) override;
        int numInitStages() const override { return NUM_INIT_STAGES; }
        void handleMessage(cMessage *msg) override;
        void finish() override;
        void refreshDisplay() const override;

        void sendRequest();
        void rescheduleOrDeleteTimer(simtime_t d, short int msgKind);

        void handleTimer(cMessage *msg) override;

        void socketEstablished(TcpSocket *socket) override;
        void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
        void socketClosed(TcpSocket *socket) override;
        void socketFailure(TcpSocket *socket, int code) override;

        void handleStartOperation(LifecycleOperation *operation) override;
        void handleStopOperation(LifecycleOperation *operation) override;
        void handleCrashOperation(LifecycleOperation *operation) override;

        void saveData(cMessage* msg) override;

        void delayedMsgSend(cMessage* msg);
        void finalMsgSend(cMessage* msg, const char* module);
        void msgReturn(cMessage* msg);

        void close() override;
};

}

#endif /* SENSORNODEBASE_H_ */
