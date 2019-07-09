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

#ifndef EXPERIMENTALCONTROL_H_
#define EXPERIMENTALCONTROL_H_

namespace inet {

class ExperimentalControl {

    static ExperimentalControl* instance;

    private:
        bool state = par("switch");
        ExperimentalControl() {}

    protected:
        int currentLayer;
        int newLayer;
        simtime_t startTime;
        simtime_t endTime;

    public:
        ~ExperimentalControl() = default;
        static ExperimentalControl* getInstance();

        bool getState();
        void setState();
        void handleMessage(cMessage* msg);
};

}

#endif /* EXPERIMENTALCONTROL_H_ */
