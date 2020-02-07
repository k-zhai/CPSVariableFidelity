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

#ifndef UDP_MASTERNODEUDP_H_
#define UDP_MASTERNODEUDP_H_

#include "ExperimentControlUDP.h"
#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

#include <vector>
#include <queue>
#include <string>
#include <ctime>

using std::vector;
using std::string;
using std::queue;

namespace inet {

class MasterNodeUDP : public ApplicationBase, public UdpSocket::ICallback {

    private:
        simsignal_t directArrival;
        simsignal_t realTime;
        simsignal_t pkLossCount;

        clock_t startClock;

    protected:
        UdpSocket socket;
        cMessage *selfMsg = nullptr;
        int numEchoed;    // just for WATCH

        bool dontFragment = false;

        enum SelfMsgKinds { START = 1, SEND, STOP };

        vector<short> master_msg_kinds = {50, 51, 52, 53};
        const char *packetName = nullptr;

        simtime_t startTime;
        simtime_t stopTime;

        int destPort = -1;

        queue<Packet> data;

        vector<string> destAddrs = {"DF1", "DF2"};
        vector<L3Address> destAddresses;
        vector<string> destAddressStr;

        const_simtime_t propagationDelay = 0.01;
        const_simtime_t frequency = 2;

        simtime_t lastDirectMsgTime = 0;

    protected:
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void initialize(int stage) override;
        virtual void handleMessageWhenUp(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay() const override;

        virtual L3Address chooseDestAddr(short msg_kind);
        virtual void sendPacket();
        virtual void setSocketOptions();

        virtual void processStart();
        virtual void processSend();
        virtual void processStop();

        virtual void handleStartOperation(LifecycleOperation *operation) override;
        virtual void handleStopOperation(LifecycleOperation *operation) override;
        virtual void handleCrashOperation(LifecycleOperation *operation) override;

        virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
        virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
        virtual void socketClosed(UdpSocket *socket) override;

        void saveData(Packet* pk);

        virtual void delayedMsgSend(cMessage* msg, int layer);
        virtual void finalMsgSend(cMessage* msg, const char* mod, int layer);
};

}

#endif /* UDP_MASTERNODEUDP_H_ */
