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

#ifndef SENSORNODEUDP_H_
#define SENSORNODEUDP_H_

#include <vector>
#include <string>
#include <queue>
#include <map>
#include <iterator>

using std::vector;
using std::string;
using std::queue;
using std::map;

#include "ExperimentControlUDP.h"
#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {

class SensorNodeUDP : public ApplicationBase, public UdpSocket::ICallback {

    private:
        simsignal_t udpArrival;
        bool ready = false;
        map<string, string> routes = {
                {"SN1", "UDPnetworksim.DF1.app[0]"},
                {"SN2", "UDPnetworksim.DF1.app[0]"},
                {"SN3", "UDPnetworksim.DF2.app[0]"},
                {"SN4", "UDPnetworksim.DF2.app[0]"}
        };

    protected:
        enum SelfMsgKinds { START = 1, SEND, STOP };

        // parameters
        vector<L3Address> destAddresses;
        vector<string> destAddressStr;
        int localPort = -1, destPort = -1;
        simtime_t startTime;
        simtime_t stopTime;
        bool dontFragment = false;
        const char *packetName = nullptr;

        const_simtime_t propagationDelay = 0.01;
        const_simtime_t frequency = 0.5;

        queue<simtime_t> udpMsgTimes;

        // state
        UdpSocket socket;
        cMessage *selfMsg = nullptr;
        long packetsLost = 0;

        // statistics
        int numSent = 0;
        int numReceived = 0;

        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void initialize(int stage) override;
        virtual void handleMessageWhenUp(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay();

        // chooses random destination address
        virtual L3Address chooseDestAddr();
        virtual void sendPacket();
        virtual void processPacket(Packet *msg);
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

        const char* getDirectDestination(const char* currentMod) const;

        virtual void delayedMsgSend(cMessage* msg, int layer);
        virtual void finalMsgSend(cMessage* msg, const char* mod, int layer);

    public:
        SensorNodeUDP();
        virtual ~SensorNodeUDP();
};

}

#endif /* SENSORNODEUDP_H_ */
