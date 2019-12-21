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

package research.simulations.TCP;

import inet.applications.contract.IApp;

simple DFNode like IApp
{
    parameters:
    	@class(inet::DFNode);
    	string localAddress = default(""); // local address; may be left empty ("")
        int localPort = default(1000);     // localPort number to listen on
        string connectAddress = default("");
        int connectPort = default(2000);
        double replyDelay @unit(s) = default(0s);
        double startTime @unit(s) = default(1s);
        double stopTime @unit(s) = default(150s);
        volatile int numRequestsPerSession = default(10);
        volatile int requestLength @unit(B) = default(200B);
        volatile int replyLength @unit(B) = default(1MiB);
        volatile double thinkTime @unit(s);
        volatile double idleInterval @unit(s);
        volatile double reconnectInterval @unit(s) = default(30s);
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // extra time after lifecycle stop operation finished
        double stopOperationTimeout @unit(s) = default(2s);    // timeout value for lifecycle stop operation
        @signal[packetSent](type=inet::Packet);
        @signal[packetReceived](type=inet::Packet);
        @signal[connect](type=long);
        @signal[directMsgArrived](type="double");
        @signal[tcpPkArrived](type="double");
        @statistic[packetReceived](title="packets received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[packetSent](title="packets sent"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="end-to-end delay"; source="dataAge(packetReceived)"; unit=s; record=histogram,weightedHistogram,vector; interpolationmode=none);
        @statistic[directTimes](title="direct msg times"; source="directMsgArrived"; record=vector,stats; interpolationmode=none);
		@statistic[tcpTimes](title="udp pk times"; source="tcpPkArrived"; record=vector,stats; interpolationmode=none);
    gates:
        input socketIn @labels(TcpCommand/up);
        output socketOut @labels(TcpCommand/down);
    	input appIn @directIn;
    	output appOut;
    	input transportIn @directIn;
    	output transportOut;
}
