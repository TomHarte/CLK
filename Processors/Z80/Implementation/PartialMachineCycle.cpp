//
//  PartialMachineCycle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "../Z80.hpp"

using namespace CPU::Z80;

PartialMachineCycle::PartialMachineCycle(const PartialMachineCycle &rhs) noexcept :
	operation(rhs.operation),
	length(rhs.length),
	address(rhs.address),
	value(rhs.value),
	was_requested(rhs.was_requested) {}

PartialMachineCycle::PartialMachineCycle(Operation operation, HalfCycles length, uint16_t *address, uint8_t *value, bool was_requested) noexcept :
	operation(operation), length(length), address(address), value(value), was_requested(was_requested)  {}

PartialMachineCycle::PartialMachineCycle() noexcept :
	operation(Internal), length(0), address(nullptr), value(nullptr), was_requested(false) {}
