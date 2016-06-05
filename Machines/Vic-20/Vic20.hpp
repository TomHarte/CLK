//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../CRTMachine.hpp"

namespace Vic20 {

class Machine: public CPU6502::Processor<Machine>, public CRTMachine::Machine {
};

}

#endif /* Vic20_hpp */
