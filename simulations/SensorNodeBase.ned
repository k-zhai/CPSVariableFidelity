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

package research.simulations;

import inet.applications.contract.IApp;

simple SensorNodeBase like IApp
{
    parameters:
        string localAddress = default("");
        int localPort = default(-1);
        string connectAddress = default("");
        int connectPort  = default(1000);
        double startTime @unit(s) = default(1s);
        double stopTime @unit(s) = default(-1s);
        double replyDelay @unit(s) = default(0s);
        volatile int numRequestsPerSession = default(1);
        volatile int requestLength @unit(B) = default(200B);
        volatile int replyLength @unit(B) = default(1MiB);
        volatile double thinkTime @unit(s);
        volatile double idleInterval @unit(s);
        volatile double reconnectInterval @unit(s) = default(30s);
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);
        double stopOperationTimeout @unit(s) = default(2s);
        @signal[packetSent](type=inet::Packet);
        @signal[connect](type=long);  // 1 for open, -1 for close
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[packetSent](title="packets sent"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="end-to-end delay"; source="dataAge(packetReceived)"; unit=s; record=histogram,weightedHistogram,vector; interpolationmode=none);
        @statistic[numActiveSessions](title="number of active sessions"; source="sum(connect)"; record=max,timeavg,vector; interpolationmode=sample-hold);
        @statistic[numSessions](title="total number of sessions"; source="sum(connect+1)/2"; record=last);
	gates:
	    input socketIn @labels(TcpCommand/up);
	    output socketOut @labels(TcpCommand/down);
	    input appIn @labels(ApplicationPacket/up) @directIn;
	    output appOut @labels(ApplicationPacket/down);
}
