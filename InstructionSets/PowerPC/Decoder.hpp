//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_PowerPC_Decoder_hpp
#define InstructionSets_PowerPC_Decoder_hpp

#include "Instruction.hpp"

namespace InstructionSet {
namespace PowerPC {

enum class Model {
	/// i.e. 32-bit, with POWER carry-over instructions.
	MPC601,
	/// i.e. 32-bit, no POWER instructions.
	MPC603,
	/// i.e. 64-bit.
	MPC620,
};

constexpr bool is64bit(Model model) {
	return model == Model::MPC620;
}

constexpr bool is32bit(Model model) {
	return !is64bit(model);
}

constexpr bool is601(Model model) {
	return model == Model::MPC601;
}

/*!
	Implements PowerPC instruction decoding.

	@c model Indicates the instruction set to decode.

	@c validate_reserved_bits If set to @c true, check that all
	reserved bits are 0 and produce an invalid opcode if not. Otherwise does no
	inspection of reserved bits.

	TODO: determine what specific models of PowerPC do re: reserved bits.
*/
template <Model model, bool validate_reserved_bits = false> struct Decoder {
	Instruction decode(uint32_t opcode);
};

}
}

#endif /* InstructionSets_PowerPC_Decoder_hpp */
