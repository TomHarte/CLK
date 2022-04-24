//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_Decoder_hpp
#define InstructionSets_M68k_Decoder_hpp

#include "Instruction.hpp"
#include "Model.hpp"

namespace InstructionSet {
namespace M68k {

/*!
	A stateless decoder that can map from instruction words to preinstructions
	(i.e. enough to know the operation and size, and either know the addressing mode
	and registers or else know how many further extension words are needed).
*/
template <Model model> class Predecoder {
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

		using OpT = uint8_t;

		// Specific instruction decoders.
		template <OpT operation, bool validate = true> Preinstruction decode(uint16_t instruction);
		template <OpT operation, bool validate> Preinstruction validated(Preinstruction original);
		template <uint8_t op> uint32_t invalid_operands();

		// Extended operation list; collapses into a single byte enough information to
		// know both the type of operation and how to decode the operands. Most of the
		// time that's knowable from the Operation alone, hence the rather awkward
		// extension of @c Operation.
		enum ExtendedOperation: OpT {
			MOVEMtoRl = uint8_t(Operation::Max) + 1, MOVEMtoRw,
			MOVEMtoMl, MOVEMtoMw,

			MOVEPtoRl, MOVEPtoRw,
			MOVEPtoMl, MOVEPtoMw,

			ADDQb,	ADDQw,	ADDQl,
			ADDQAw,	ADDQAl,
			SUBQb,	SUBQw,	SUBQl,
			SUBQAw,	SUBQAl,

			ADDIb,	ADDIw,	ADDIl,
			ORIb,	ORIw,	ORIl,
			SUBIb,	SUBIw,	SUBIl,
			ANDIb,	ANDIw,	ANDIl,
			EORIb,	EORIw,	EORIl,
			CMPIb,	CMPIw,	CMPIl,

			BTSTI, BCHGI, BCLRI, BSETI,

			CMPMb,	CMPMw,	CMPMl,

			MOVEq,

			ADDtoMb, ADDtoMw, ADDtoMl,
			ADDtoRb, ADDtoRw, ADDtoRl,

			SUBtoMb, SUBtoMw, SUBtoMl,
			SUBtoRb, SUBtoRw, SUBtoRl,

			ANDtoMb, ANDtoMw, ANDtoMl,
			ANDtoRb, ANDtoRw, ANDtoRl,

			ORtoMb, ORtoMw, ORtoMl,
			ORtoRb, ORtoRw, ORtoRl,
		};

		static constexpr Operation operation(OpT op);
};

}
}

#endif /* InstructionSets_M68k_Decoder_hpp */
