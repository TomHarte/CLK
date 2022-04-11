//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef Decoder_hpp
#define Decoder_hpp

#include "Instruction.hpp"

namespace InstructionSet {
namespace M68k {

/*!
	A stateless decoder that can map from instruction words to preinstructions
	(i.e. enough to know the operation and size, and either know the addressing mode
	and registers or else know how many further extension words are needed).
*/
class Predecoder {
	public:
		Preinstruction decode(uint16_t instruction);

	private:
		// Page by page decoders; each gets a bit ad hoc so
		// it is neater to separate them.
		Preinstruction decode8(uint16_t instruction);
		Preinstruction decodeC(uint16_t instruction);

		// Specific instruction decoders.
		template <Operation operation> Preinstruction decode(uint16_t instruction);
};

}
}

#endif /* Decoder_hpp */
