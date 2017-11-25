//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include "../../Processors/Z80/Z80.hpp"

namespace MSX {

class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine {
	public:
		ConcreteMachine():
			z80_(*this) {}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
