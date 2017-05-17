//
//  Z80AllRAM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "Z80AllRAM.hpp"
#include <algorithm>

using namespace CPU::Z80;

AllRAMProcessor::AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}

int AllRAMProcessor::perform_machine_cycle(const MachineCycle *cycle) {
	return 0;
}
