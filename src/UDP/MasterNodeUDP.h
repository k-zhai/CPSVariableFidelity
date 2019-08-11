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
#include <string>

using std::vector;
using std::string;

namespace inet {

class MasterNodeUDP : public ApplicationBase, public UdpSocket::ICallback {

    protected:
        UdpSocket socket;
        int numEchoed;    // just for WATCH

        vector<string> data;
        const vector<string> targets = {"DF1", "DF2"};

        const_simtime_t propagationDelay = 0.01;
        const_simtime_t frequency = 2;

        simtime_t lastDirectMsgTime = 0;

    protected:
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void initialize(int stage) override;
        virtual void handleMessageWhenUp(cMessage *msg) override;
        virtual void finish() override;
        virtual void refreshDisplay() const override;

        virtual void handleStartOperation(LifecycleOperation *operation) override;
        virtual void handleStopOperation(LifecycleOperation *operation) override;
        virtual void handleCrashOperation(LifecycleOperation *operation) override;

        virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
        virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
        virtual void socketClosed(UdpSocket *socket) override;

        void saveData(cMessage* msg);

        virtual void delayedMsgSend(cMessage* msg, int layer);
        virtual void finalMsgSend(cMessage* msg, const char* mod, int layer);
};

}

#endif /* UDP_MASTERNODEUDP_H_ */
