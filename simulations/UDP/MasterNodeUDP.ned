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

package research.simulations.UDP;

import inet.applications.contract.IApp;

simple MasterNodeUDP like IApp
{
    parameters:
        @class(inet::MasterNodeUDP);
        string interfaceTableModule;
        int localPort;
        int destPort;
        string localAddress = default("");
        double startTime @unit(s) = default(this.sendInterval); // application start time (start of the first packet)
        double stopTime @unit(s) = default(-1s);  // time of finishing sending, -1s means forever
        volatile double sendInterval @unit(s); // should usually be a random value, e.g. exponential(1)
        volatile int messageLength @unit(B); // length of messages to generate, in bytes
        string packetName = default("UdpBasicAppData");
        int timeToLive = default(5);
        int typeOfService = default(-1);
        bool dontFragment = default(false);
        string multicastInterface = default("");  // if not empty, set the multicast output interface option on the socket (interface name expected)
        bool receiveBroadcast = default(false); // if true, makes the socket receive broadcast packets
        bool joinLocalMulticastGroups = default(false); // if true, makes the socket receive packets from all multicast groups set on local interfaces
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);
        double stopOperationTimeout @unit(s) = default(2s);
        @signal[packetSent](type=inet::Packet);
        @signal[directMsgArrived](type="double");
        @signal[recordWallTime](type="double");
        @signal[totalPacketsLost](type="long");
        @statistic[echoedPk](title="packets echoed"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
    	@statistic[directTimes](title="direct msg times"; source="directMsgArrived"; record=histogram,vector,stats; interpolationmode=none);
    	@statistic[wallTimes](title="wall time vs logical time"; source="recordWallTime"; record=vector; interpolationmode=none);
    	@statistic[packetLost](title="total packets lost"; source="totalPacketsLost"; record=vector; interpolationmode=none);
    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
        input appIn @directIn;
	    output appOut;
	    input transportIn @directIn;
	    output transportOut;
}
