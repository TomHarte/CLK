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
#include "../../Numeric/Sizes.hpp"

namespace InstructionSet {
namespace M68k {

/*!
	A stateless decoder that can map from instruction words to preinstructions
	(i.e. enough to know the operation and size, and either know the addressing mode
	and registers or else know how many further extension words are needed).

	WARNING: at present this handles the original 68000 instruction set only. It
	requires a model only for the sake of not baking in assumptions about MOVE SR, etc,
	and supporting extended addressing modes in some cases.

	But it does not yet decode any operations which were not present on the 68000.
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

		// Yuckiness here: 67 is a count of the number of things contained below in
		// ExtendedOperation; this acts to ensure ExtendedOperation is the minimum
		// integer size large enough to hold all actual operations plus the ephemeral
		// ones used here. Intention is to support table-based decoding, which will mean
		// making those integers less ephemeral, hence the desire to pick a minimum size.
		using OpT = typename MinIntTypeValue<
			uint64_t(OperationMax<model>::value) + 67
		>::type;
		static constexpr auto OpMax = OpT(OperationMax<model>::value);

		// Specific instruction decoders.
		template <OpT operation, bool validate = true> Preinstruction decode(uint16_t instruction);
		template <OpT operation, bool validate> Preinstruction validated(
			AddressingMode op1_mode = AddressingMode::None, int op1_reg = 0,
			AddressingMode op2_mode = AddressingMode::None, int op2_reg = 0,
			Condition condition = Condition::True
		);
		template <OpT operation> uint32_t invalid_operands();

		// Extended operation list; collapses into a single byte enough information to
		// know both the type of operation and how to decode the operands. Most of the
		// time that's knowable from the Operation alone, hence the rather awkward
		// extension of @c Operation.
		enum ExtendedOperation: OpT {
			MOVEPtoRl = OpMax + 1, MOVEPtoRw,
			MOVEPtoMl, MOVEPtoMw,

			MOVEQ,

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

			ADDtoMb, ADDtoMw, ADDtoMl,
			ADDtoRb, ADDtoRw, ADDtoRl,

			SUBtoMb, SUBtoMw, SUBtoMl,
			SUBtoRb, SUBtoRw, SUBtoRl,

			ANDtoMb, ANDtoMw, ANDtoMl,
			ANDtoRb, ANDtoRw, ANDtoRl,

			ORtoMb, ORtoMw, ORtoMl,
			ORtoRb, ORtoRw, ORtoRl,

			EXGRtoR, EXGAtoA, EXGRtoA,
		};

		static constexpr Operation operation(OpT op);
};

}
}

#endif /* InstructionSets_M68k_Decoder_hpp */
