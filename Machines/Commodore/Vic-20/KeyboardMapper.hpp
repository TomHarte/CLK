//
//  KeyboardMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_Commodore_Vic20_KeyboardMapper_hpp
#define Machines_Commodore_Vic20_KeyboardMapper_hpp

#include "../../KeyboardMachine.hpp"

namespace Commodore {
namespace Vic20 {

struct KeyboardMapper: public KeyboardMachine::Machine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key);
};

}
}

#endif /* KeyboardMapper_hpp */
