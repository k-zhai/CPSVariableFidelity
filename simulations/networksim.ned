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

network networksim
{
    parameters:
        @display("bgb=882,507");
        double per = default(0);
    types:
        channel C extends DatarateChannel
        {
            datarate = 100000bps;
            delay = 0.02s; //propagation delay
            per = per;
        }
    submodules:
        client: newStandardHost {
            parameters:
                @display("p=114,223;i=device/pc");
        }
        server: newStandardHost {
            parameters:
                @display("p=714,223;i=device/server");
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
        client.pppg++ <--> C <--> server.pppg++;

}
