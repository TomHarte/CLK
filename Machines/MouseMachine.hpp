//
//  MouseMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MouseMachine_hpp
#define MouseMachine_hpp

#include "../Inputs/Mouse.hpp"

namespace MachineTypes {

class MouseMachine {
	public:
		// TODO: support multiple mice?
		virtual Inputs::Mouse &get_mouse() = 0;
};

}

#endif /* MouseMachine_hpp */
