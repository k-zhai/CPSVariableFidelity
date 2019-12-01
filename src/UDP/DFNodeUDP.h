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

#ifndef DFNODEUDP_H_
#define DFNODEUDP_H_

#include "ExperimentControlUDP.h"
#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

#include <vector>
#include <string>
#include <queue>

using std::vector;
using std::string;
using std::queue;

namespace inet {

class DFNodeUDP : public ApplicationBase, public UdpSocket::ICallback {

    private:
        simsignal_t directArrival;
        simsignal_t udpArrival;
        bool ready = false;

    protected:
        enum SelfMsgKinds { START = 1, SEND, STOP };

        UdpSocket socket;
        cMessage *selfMsg = nullptr;
        long packetsLost = 0;

        vector<L3Address> destAddresses;
        vector<string> destAddressStr;
        int localPort = -1, destPort = -1;
        simtime_t startTime;
        simtime_t stopTime;
        bool dontFragment = false;
        const char *packetName = nullptr;

        vector<string> data;
        const vector<string> DF1targets = {"SN1", "SN2"};
        const vector<string> DF2targets = {"SN3", "SN4"};

        const_simtime_t propagationDelay = 0.01;
        const_simtime_t frequency = 2;

        simtime_t lastDirectMsgTime = 0;
        queue<simtime_t> udpMsgTimes;

        int numEchoed;
        int numSent = 0;
        int numReceived = 0;

        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void initialize(int stage) override;
        virtual void handleMessageWhenUp(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay() const override;

        void handleDirectMessage(cMessage *msg);
        void saveData(cMessage* msg);

        virtual void delayedMsgSend(cMessage* msg, int layer);
        virtual void finalMsgSend(cMessage* msg, const char* mod, int layer);

        void finalMsgSendRouter(cMessage* msg, const char* currentMod);

        virtual void handleStartOperation(LifecycleOperation *operation) override;
        virtual void handleStopOperation(LifecycleOperation *operation) override;
        virtual void handleCrashOperation(LifecycleOperation *operation) override;

        virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
        virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
        virtual void socketClosed(UdpSocket *socket) override;

        /* ------------------------------------------------------------------------------------- */

        virtual L3Address chooseDestAddr();
        virtual void sendPacket();
        virtual void processPacket(Packet *msg);
        virtual void setSocketOptions();

        virtual void processStart();
        virtual void processSend();
        virtual void processStop();

    public:
        DFNodeUDP();
        virtual ~DFNodeUDP();
};

}

#endif /* DFNODEUDP_H_ */
