//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

namespace MSX {

class ConcreteMachine:
	public Machine {
	
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
