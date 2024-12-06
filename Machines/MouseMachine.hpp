//
//  MouseMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Inputs/Mouse.hpp"

namespace MachineTypes {

struct MouseMachine {
	// TODO: support multiple mice?
	virtual Inputs::Mouse &get_mouse() = 0;
};

}
