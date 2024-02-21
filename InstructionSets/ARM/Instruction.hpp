//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Decoder.hpp"
#include "Model.hpp"
#include "Operation.hpp"

namespace InstructionSet::ARM {

/*

		//
		// LDR and STR.
		//
		struct SingleDataTransfer {
			constexpr SingleDataTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

			/// The destination register index. i.e. 'Rd' for LDR.
			int destination() const				{	return (opcode_ >> 12) & 0xf;	}

			/// The destination register index. i.e. 'Rd' for STR.
			int source() const					{	return (opcode_ >> 12) & 0xf;	}

			/// The base register index. i.e. 'Rn'.
			int base() const					{	return (opcode_ >> 16) & 0xf;	}

			///
			int offset() const					{	return opcode_ & 0xfff;			}

			// TODO: P, U, B, W, L, I.
*/

}
