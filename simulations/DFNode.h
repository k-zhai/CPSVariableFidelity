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

#ifndef DFNODE_H_
#define DFNODE_H_

#include "../SensorNodeBase.h"
#include <vector>
#include <string>

namespace inet {

class INET_API DFNode: public SensorNodeBase {

    protected:

        std::vector<std::string> data;

        void saveData(cMessage* msg);

};

}

#endif /* DFNODE_H_ */
