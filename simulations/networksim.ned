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

import inet.common.*;
import inet.common.misc.NetAnimTrace;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import ned.DatarateChannel;
import *.*.newStandardHost;
import *.*.ExperimentalControl;

network networksim
{
    parameters:
        @display("bgb=1003.39496,554.8575");
        double per = default(0);
    types:
        channel C extends DatarateChannel
        {
            datarate = 100000bps;
            delay = 0.02s; //propagation delay
            per = per;
        }
    submodules:
        SN1: newStandardHost {
            parameters:
                @display("p=114,223;i=device/pc");
        }
        SN2: newStandardHost {
            parameters:
                @display("p=714,223;i=device/pc");
        }
        DF1: newStandardHost {
            parameters:
                @display("p=466.2575,355.5075");
        }
        M: newStandardHost {
            parameters:
                @display("i=device/pc;p=287.94998,421.9575");
        }
        EC: ExperimentalControl {
            parameters:
                @display("i=device/pc;p=112.965,421.9575");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=402,100");
        }
        netAnimTrace: NetAnimTrace {
            parameters:
                @display("p=227.03749,99.674995;is=s");
        }
    connections:
        SN1.pppg++ <--> C <--> SN2.pppg++;
//        Client.appOut --> Server.appIn;
//        Client.appIn <-- Server.appOut;
//        Client.socketOut --> C --> Server.socketIn;
//        Client.socketIn <-- C <-- Server.socketOut;

}
