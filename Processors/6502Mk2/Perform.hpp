//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"
#include "Registers.hpp"

#pragma once

namespace CPU::MOS6502Mk2 {

template <typename RegistersT>
void perform(const Operation operation, RegistersT &registers, const uint8_t operand) {
	(void)operation;
	(void)registers;
	(void)operand;

	// TODO.
}

}
