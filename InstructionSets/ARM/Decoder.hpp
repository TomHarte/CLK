//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Model.hpp"
#include "Operation.hpp"

#include <array>

namespace InstructionSet::ARM {

template <Model model>
constexpr std::array<Operation, 256> operation_table() {
	std::array<Operation, 256> result{};
	for(int c = 0; c < 256; c++) {
		const uint32_t opcode = c << 20;

		if(((opcode >> 26) & 0b11) == 0b00) {
			result[c] = Operation((c >> 21) & 0xf);
			continue;
		}

		if(((opcode >> 25) & 0b111) == 0b101) {
			result[c] =
				((opcode >> 24) & 1) ? Operation::BranchWithLink : Operation::Branch;
			continue;
		}

		if(((opcode >> 22) & 0b111'111) == 0b000'000) {
			result[c] =
				((opcode >> 21) & 1) ? Operation::MultiplyWithAccumulate : Operation::Multiply;
			continue;
		}

		if(((opcode >> 26) & 0b11) == 0b01) {
			result[c] = Operation::SingleDataTransfer;
			continue;
		}

		if(((opcode >> 25) & 0b111) == 0b100) {
			result[c] = Operation::BlockDataTransfer;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1111) {
			result[c] = Operation::SoftwareInterrupt;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1110) {
			result[c] = Operation::CoprocessorDataOperationOrRegisterTransfer;
			continue;
		}

		if(((opcode >> 25) & 0b111) == 0b110) {
			result[c] = Operation::CoprocessorDataTransfer;
			continue;
		}

		result[c] = Operation::Undefined;
	}
	return result;
}

template <Model model>
constexpr Operation operation(uint32_t opcode) {
	constexpr std::array<Operation, 256> operations = operation_table<model>();
	return operations[(opcode >> 20) & 0xff];
}

}
