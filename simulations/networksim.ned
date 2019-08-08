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

import inet.common.misc.NetAnimTrace;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import ned.DatarateChannel;
import research.simulations.*;

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
                @display("p=166.155,147.27376;i=device/pc");
        }
        SN2: newStandardHost {
            parameters:
                @display("p=412.87003,115.80501;i=device/pc");
        }
        DF1: newStandardHost {
            parameters:
                @display("i=device/pc;p=290.77127,326.01627");
        }
        SN3: newStandardHost {
            parameters:
                @display("i=device/pc;p=512.3113,62.937504");
        }
        SN4: newStandardHost {
            parameters:
                @display("i=device/pc;p=881.12506,62.937504");
        }
        DF2: newStandardHost {
            parameters:
                @display("i=device/pc;p=696.08875,147.27376");
        }
        M: newStandardHost {
            parameters:
                @display("i=device/pc;p=607.97626,399.02377");
        }
        EC: ExperimentControl {
            parameters:
                @display("i=device/pc;p=166.155,47.8325");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=47.8325,47.8325");
        }
        netAnimTrace: NetAnimTrace {
            parameters:
                @display("p=47.8325,137.20375;is=s");
        }
    connections:
        SN1.pppg++ <--> C <--> DF1.pppg++;
        SN2.pppg++ <--> C <--> DF1.pppg++;
        SN3.pppg++ <--> C <--> DF2.pppg++;
        SN4.pppg++ <--> C <--> DF2.pppg++;
		DF1.pppg++ <--> C <--> M.pppg++;
		DF2.pppg++ <--> C <--> M.pppg++;

}
