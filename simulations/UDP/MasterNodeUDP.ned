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
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);
        double stopOperationTimeout @unit(s) = default(2s);
        @signal[packetSent](type=inet::Packet);
        @signal[directMsgArrived](type="double");
        @statistic[echoedPk](title="packets echoed"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
    	@statistic[directTimes](title="direct msg times"; source="directMsgArrived"; record=histogram,vector,stats; interpolationmode=none);
    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
        input appIn @directIn;
	    output appOut;
}
