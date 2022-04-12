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
		Preinstruction decode0(uint16_t instruction);
		Preinstruction decode1(uint16_t instruction);
		Preinstruction decode2(uint16_t instruction);
		Preinstruction decode3(uint16_t instruction);
		Preinstruction decode4(uint16_t instruction);
		Preinstruction decode5(uint16_t instruction);
		Preinstruction decode6(uint16_t instruction);
		Preinstruction decode7(uint16_t instruction);
		Preinstruction decode8(uint16_t instruction);
		Preinstruction decode9(uint16_t instruction);
		Preinstruction decodeA(uint16_t instruction);
		Preinstruction decodeB(uint16_t instruction);
		Preinstruction decodeC(uint16_t instruction);
		Preinstruction decodeD(uint16_t instruction);
		Preinstruction decodeE(uint16_t instruction);
		Preinstruction decodeF(uint16_t instruction);

		// Specific instruction decoders.
		template <uint8_t operation> Preinstruction decode(uint16_t instruction);

		enum ExtendedOperation {
			MOVEMtoRl = uint8_t(Operation::Max), MOVEMtoRw,
			MOVEMtoMl, MOVEMtoMw,

			MOVEPtoRl, MOVEPtoRw,
			MOVEPtoMl, MOVEPtoMw,

		};
		static constexpr Operation operation(uint8_t op);
};

}
}

#endif /* Decoder_hpp */
