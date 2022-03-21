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

	This is an experimental implementation; it has not yet undergone significant testing.
*/
struct Decoder {
	public:
		Decoder(Model model);

		Instruction decode(uint32_t opcode);

	private:
		Model model_;
};

}
}

#endif /* InstructionSets_PowerPC_Decoder_hpp */
