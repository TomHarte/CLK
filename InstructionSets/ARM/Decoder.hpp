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
using OperationTable = std::array<Operation, 256>;

template <Model model>
constexpr OperationTable<model> operation_table() {
	OperationTable<model> result{};
	for(std::size_t c = 0; c < result.size(); c++) {
		const auto opcode = static_cast<uint32_t>(c << 20);

		// Cf. the ARM2 datasheet, p.45. Tests below match its ordering
		// other than that 'undefined' is the fallthrough case. More specific
		// page references are provided were more detailed versions of the
		// decoding are depicted.

		// Data processing; cf. p.17.
		if(((opcode >> 26) & 0b11) == 0b00) {
			result[c] = Operation((c >> 21) & 0xf);
			continue;
		}

		// Multiply and multiply-accumulate (MUL, MLA); cf. p.23.
		if(((opcode >> 22) & 0b111'111) == 0b000'000) {
			result[c] =
				((opcode >> 21) & 1) ? Operation::MLA : Operation::MUL;
			continue;
		}

		// Single data transfer (LDR, STR); cf. p.25.
		if(((opcode >> 26) & 0b11) == 0b01) {
			result[c] =
				((opcode >> 20) & 1) ? Operation::LDR : Operation::STR;
			continue;
		}

		// Block data transfer (LDM, STM); cf. p.29.
		if(((opcode >> 25) & 0b111) == 0b100) {
			result[c] =
				((opcode >> 20) & 1) ? Operation::LDM : Operation::STM;
			continue;
		}

		// Branch and branch with link (B, BL); cf. p.15.
		if(((opcode >> 25) & 0b111) == 0b101) {
			result[c] =
				((opcode >> 24) & 1) ? Operation::BL : Operation::B;
			continue;
		}

		if(((opcode >> 25) & 0b111) == 0b110) {
			result[c] = Operation::CoprocessorDataTransfer;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1110) {
			result[c] = Operation::CoprocessorDataOperationOrRegisterTransfer;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1111) {
			result[c] = Operation::SoftwareInterrupt;
			continue;
		}

		result[c] = Operation::Undefined;
	}
	return result;
}

template <Model model>
constexpr Operation operation(uint32_t opcode) {
	constexpr OperationTable<model> operations = operation_table<model>();

	const auto op = operations[(opcode >> 21) & 0x7f];

	// MUL and MLA have an extra constraint that doesn't fit the neat
	// 256-entry table format as above.
	if(
		(op == Operation::MUL || op == Operation::MLA)
		&& ((opcode >> 4) & 0b1111) != 0b1001
	) {
		return Operation::Undefined;
	}

	return op;
}

}
