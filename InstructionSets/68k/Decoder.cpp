//
//  Decoder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

#include <cassert>

using namespace InstructionSet::M68k;

namespace {

constexpr AddressingMode extended_modes[] = {
	AddressingMode::AbsoluteShort,										// 1'000
	AddressingMode::AbsoluteLong,										// 1'001
	AddressingMode::ProgramCounterIndirectWithDisplacement,				// 1'010
	AddressingMode::ProgramCounterIndirectWithIndex8bitDisplacement,	// 1'011
	AddressingMode::ImmediateData,										// 1'100
	AddressingMode::None,												// [1'101]
	AddressingMode::None,												// [1'110]
	AddressingMode::None,												// [1'111]
};

/// @returns The @c AddressingMode given the specified mode and reg, subject to potential
/// 	aliasing on the '020+ as described above the @c AddressingMode enum.
constexpr AddressingMode combined_mode(int mode, int reg) {
	assert(mode >= 0 && mode < 8);
	assert(reg >= 0 && reg < 8);

	// Look up mode, mapping invalid combinations to ::None, branchlessly.
	const int use_reg = ((mode + 1) >> 3) & 1;
	const AddressingMode modes[] = { AddressingMode(mode), extended_modes[reg] };
	return modes[use_reg];
}

template <AddressingMode F> struct Mask {
	static constexpr uint32_t value = uint32_t(1 << int(F));
};

static constexpr uint32_t NoOperand = Mask<AddressingMode::None>::value;

template <uint32_t first, uint32_t second> struct TwoOperandMask {
	static constexpr uint32_t value = ((first << 16) & 0xffff0000) | (second & 0x0000ffff);
};

template <uint32_t first> struct OneOperandMask {
	static constexpr uint32_t value = TwoOperandMask<first, NoOperand>::value;
};

struct NoOperandMask {
	static constexpr uint32_t value = OneOperandMask<NoOperand>::value;
};

uint32_t operand_mask(AddressingMode mode1, AddressingMode mode2) {
	return uint32_t((0x1'0000 << int(mode1)) | (0x0'0001 << int(mode2)));
}

}

// MARK: - Instruction decoders.

/// Maps from an ExtendedOperation to an Operation; in practice that means that anything
/// that already is an Operation is passed through, and other things are mapped down into
/// an operation that doesn't duplicate detail about the operands that can be held by a
/// Preinstruction in other ways — for example, ANDI and AND are both represented by
/// a Preinstruction with an operation of AND, the former just happens to specify an
/// immediate operand.
template <Model model>
constexpr Operation Predecoder<model>::operation(OpT op) {
	if(op < OpT(Operation::Max)) {
		return Operation(op);
	}

	switch(op) {
		case MOVEMtoRl:	case MOVEMtoMl:	return Operation::MOVEMl;
		case MOVEMtoRw:	case MOVEMtoMw:	return Operation::MOVEMw;
		case MOVEPtoRl:	case MOVEPtoMl:	return Operation::MOVEPl;
		case MOVEPtoRw:	case MOVEPtoMw:	return Operation::MOVEPw;

		case ADDQb:		return Operation::ADDb;
		case ADDQw:		return Operation::ADDw;
		case ADDQl:		return Operation::ADDl;
		case ADDQAw:	return Operation::ADDAw;
		case ADDQAl:	return Operation::ADDAl;
		case SUBQb:		return Operation::SUBb;
		case SUBQw:		return Operation::SUBw;
		case SUBQl:		return Operation::SUBl;
		case SUBQAw:	return Operation::SUBAw;
		case SUBQAl:	return Operation::SUBAl;

		case BTSTI:		return Operation::BTST;
		case BCHGI:		return Operation::BCHG;
		case BCLRI:		return Operation::BCLR;
		case BSETI:		return Operation::BSET;

		case CMPMb:		return Operation::CMPb;
		case CMPMw:		return Operation::CMPw;
		case CMPMl:		return Operation::CMPl;

#define ImmediateGroup(x)	\
		case x##Ib:		return Operation::x##b;	\
		case x##Iw:		return Operation::x##w;	\
		case x##Il:		return Operation::x##l;

		ImmediateGroup(ADD)
		ImmediateGroup(SUB);
		ImmediateGroup(OR);
		ImmediateGroup(AND);
		ImmediateGroup(EOR);
		ImmediateGroup(CMP);

#undef ImmediateGroup

		case ADDtoRb:	case ADDtoMb:	return Operation::ADDb;
		case ADDtoRw:	case ADDtoMw:	return Operation::ADDw;
		case ADDtoRl:	case ADDtoMl:	return Operation::ADDl;

		case SUBtoRb:	case SUBtoMb:	return Operation::SUBb;
		case SUBtoRw:	case SUBtoMw:	return Operation::SUBw;
		case SUBtoRl:	case SUBtoMl:	return Operation::SUBl;

		case ANDtoRb:	case ANDtoMb:	return Operation::ANDb;
		case ANDtoRw:	case ANDtoMw:	return Operation::ANDw;
		case ANDtoRl:	case ANDtoMl:	return Operation::ANDl;

		case ORtoRb:	case ORtoMb:	return Operation::ORb;
		case ORtoRw:	case ORtoMw:	return Operation::ORw;
		case ORtoRl:	case ORtoMl:	return Operation::ORl;

		case EXGRtoR:	case EXGAtoA:	case EXGRtoA:	return Operation::EXG;

		case MOVEQ:		return Operation::MOVEl;

		default: break;
	}

	return Operation::Undefined;
}

template <Model model>
template <uint8_t op> uint32_t Predecoder<model>::invalid_operands() {
	constexpr auto Dn		= Mask< AddressingMode::DataRegisterDirect >::value;
	constexpr auto An		= Mask< AddressingMode::AddressRegisterDirect >::value;
	constexpr auto Ind		= Mask< AddressingMode::AddressRegisterIndirect >::value;
	constexpr auto PostInc	= Mask< AddressingMode::AddressRegisterIndirectWithPostincrement >::value;
	constexpr auto PreDec	= Mask< AddressingMode::AddressRegisterIndirectWithPredecrement >::value;
	constexpr auto d16An	= Mask< AddressingMode::AddressRegisterIndirectWithDisplacement >::value;
	constexpr auto d8AnXn	= Mask< AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement >::value;
	constexpr auto XXXw		= Mask< AddressingMode::AbsoluteShort >::value;
	constexpr auto XXXl		= Mask< AddressingMode::AbsoluteLong >::value;
	constexpr auto d16PC	= Mask< AddressingMode::ProgramCounterIndirectWithDisplacement >::value;
	constexpr auto d8PCXn	= Mask< AddressingMode::ProgramCounterIndirectWithIndex8bitDisplacement >::value;
	constexpr auto Imm		= Mask< AddressingMode::ImmediateData >::value;
	constexpr auto Quick	= Mask< AddressingMode::Quick >::value;

	// A few recurring combinations; terminology is directly from
	// the Programmers' Reference Manual.

	//
	// All modes: the complete set (other than Quick).
	//
	static constexpr auto AllModes 		= Dn | An | Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl | d16PC | d8PCXn | Imm;
	static constexpr auto AllModesNoAn 	= AllModes & ~An;

	//
	// Alterable addressing modes (with and without AddressRegisterDirect).
	//
	static constexpr auto AlterableAddressingModes 		= Dn | An | Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl;
	static constexpr auto AlterableAddressingModesNoAn	= AlterableAddressingModes & ~An;

	//
	// Control [flow] addressing modes.
	//
	static constexpr auto ControlAddressingModes	=  Ind | d16An | d8AnXn | XXXw | XXXl | d16PC | d8PCXn;

	switch(op) {
		// By default, disallow all operands (including 'None'). This should catch any
		// opcodes that are unmapped below.
		default: return uint32_t(~0);

		case OpT(Operation::ABCD):
		case OpT(Operation::ADDXb):	case OpT(Operation::ADDXw):	case OpT(Operation::ADDXl):
		case OpT(Operation::SBCD):
		case OpT(Operation::SUBXb):	case OpT(Operation::SUBXw):	case OpT(Operation::SUBXl):
			return ~TwoOperandMask<
				Dn | PreDec,
				Dn | PreDec
			>::value;

		case ADDtoRb:
		case ANDtoRb:	case ANDtoRw:	case ANDtoRl:
		case OpT(Operation::CHK):
		case OpT(Operation::CMPb):
		case OpT(Operation::DIVU):		case OpT(Operation::DIVS):
		case ORtoRb:	case ORtoRw:	case ORtoRl:
		case OpT(Operation::MULU):		case OpT(Operation::MULS):
		case SUBtoRb:
			return ~TwoOperandMask<
				AllModesNoAn,
				Dn
			>::value;

		case ADDtoRw:	case ADDtoRl:
		case OpT(Operation::CMPw):	case OpT(Operation::CMPl):
		case SUBtoRw:	case SUBtoRl:
			return ~TwoOperandMask<
				AllModes,
				Dn
			>::value;

		case ADDtoMb:	case ADDtoMw:	case ADDtoMl:
		case ANDtoMb:	case ANDtoMw:	case ANDtoMl:
		case ORtoMb:	case ORtoMw:	case ORtoMl:
		case SUBtoMb:	case SUBtoMw:	case SUBtoMl:
			return ~TwoOperandMask<
				Dn,
				Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl
			>::value;

		case OpT(Operation::ADDAw):		case OpT(Operation::ADDAl):
		case OpT(Operation::CMPAw):	 	case OpT(Operation::CMPAl):
		case OpT(Operation::SUBAw):		case OpT(Operation::SUBAl):
		case OpT(Operation::MOVEAw):	case OpT(Operation::MOVEAl):
			return ~TwoOperandMask<
				AllModes,
				An
			>::value;

		case ADDIb:		case ADDIl:		case ADDIw:
		case ANDIb:		case ANDIl:		case ANDIw:
		case BCHGI:		case BCLRI:		case BSETI:
		case EORIb:		case EORIw:		case EORIl:
		case ORIb:		case ORIw:		case ORIl:
		case SUBIb:		case SUBIl:		case SUBIw:
			return ~TwoOperandMask<
				Imm,
				AlterableAddressingModesNoAn
			>::value;

		case ADDQb:
		case SUBQb:
			return ~TwoOperandMask<
				Quick,
				AlterableAddressingModesNoAn
			>::value;

		case MOVEQ:
			return ~TwoOperandMask<
				Quick,
				Dn
			>::value;

		case ADDQw:	case ADDQl:
		case SUBQw:	case SUBQl:
			return ~TwoOperandMask<
				Quick,
				AlterableAddressingModes
			>::value;

		case OpT(Operation::MOVEb):
			return ~TwoOperandMask<
				AllModesNoAn,
				AlterableAddressingModesNoAn
			>::value;

		case OpT(Operation::MOVEw):	case OpT(Operation::MOVEl):
			return ~TwoOperandMask<
				AllModes,
				AlterableAddressingModesNoAn
			>::value;

		case OpT(Operation::ANDItoCCR):	case OpT(Operation::ANDItoSR):
		case OpT(Operation::Bccw):		case OpT(Operation::Bccl):
		case OpT(Operation::BSRl):		case OpT(Operation::BSRw):
		case OpT(Operation::EORItoCCR):	case OpT(Operation::EORItoSR):
		case OpT(Operation::ORItoCCR):	case OpT(Operation::ORItoSR):
		case OpT(Operation::STOP):
			return ~OneOperandMask<
				Imm
			>::value;

		case OpT(Operation::ASLb):	case OpT(Operation::ASLw):	case OpT(Operation::ASLl):
		case OpT(Operation::ASRb):	case OpT(Operation::ASRw):	case OpT(Operation::ASRl):
		case OpT(Operation::LSLb):	case OpT(Operation::LSLw):	case OpT(Operation::LSLl):
		case OpT(Operation::LSRb):	case OpT(Operation::LSRw):	case OpT(Operation::LSRl):
		case OpT(Operation::ROLb):	case OpT(Operation::ROLw):	case OpT(Operation::ROLl):
		case OpT(Operation::RORb):	case OpT(Operation::RORw):	case OpT(Operation::RORl):
		case OpT(Operation::ROXLb):	case OpT(Operation::ROXLw):	case OpT(Operation::ROXLl):
		case OpT(Operation::ROXRb):	case OpT(Operation::ROXRw):	case OpT(Operation::ROXRl):
			return ~TwoOperandMask<
				Quick | Dn,
				Dn
			>::value;

		case OpT(Operation::ASLm):
		case OpT(Operation::ASRm):
		case OpT(Operation::LSLm):
		case OpT(Operation::LSRm):
		case OpT(Operation::ROLm):
		case OpT(Operation::RORm):
		case OpT(Operation::ROXLm):
		case OpT(Operation::ROXRm):
			return ~OneOperandMask<
				Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl
			>::value;

		case OpT(Operation::Bccb):
		case OpT(Operation::BSRb):
		case OpT(Operation::TRAP):
			return ~OneOperandMask<
				Quick
			>::value;

		case OpT(Operation::BCHG):
		case OpT(Operation::BCLR):
		case OpT(Operation::BSET):
		case OpT(Operation::EORb):	case OpT(Operation::EORw):	case OpT(Operation::EORl):
			return ~TwoOperandMask<
				Dn,
				AlterableAddressingModesNoAn
			>::value;

		case OpT(Operation::BTST):
			return ~TwoOperandMask<
				Dn,
				AllModesNoAn
			>::value;

		case CMPIb:		case CMPIl:		case CMPIw:
			if constexpr (model == Model::M68000) {
				return ~TwoOperandMask<
					Imm,
					Dn | Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl
				>::value;
			}
			[[fallthrough]];
		case BTSTI:
			return ~TwoOperandMask<
				Imm,
				Dn | Ind | PostInc | PreDec | d16An | d8AnXn | XXXw | XXXl | d16PC | d8PCXn
			>::value;

		case OpT(Operation::CLRb):	case OpT(Operation::CLRw):	case OpT(Operation::CLRl):
		case OpT(Operation::NBCD):
		case OpT(Operation::MOVEfromSR):
		case OpT(Operation::NEGXb):	case OpT(Operation::NEGXw):	case OpT(Operation::NEGXl):
		case OpT(Operation::NEGb):	case OpT(Operation::NEGw):	case OpT(Operation::NEGl):
		case OpT(Operation::NOTb):	case OpT(Operation::NOTw):	case OpT(Operation::NOTl):
		case OpT(Operation::Scc):
		case OpT(Operation::TAS):
			return ~OneOperandMask<
				AlterableAddressingModesNoAn
			>::value;

		case OpT(Operation::TSTb):
			if constexpr (model == Model::M68000) {
				return ~OneOperandMask<
					AlterableAddressingModesNoAn
				>::value;
			}
			[[fallthrough]];
		case OpT(Operation::MOVEtoCCR):
		case OpT(Operation::MOVEtoSR):
			return ~OneOperandMask<
				AllModesNoAn
			>::value;

		case OpT(Operation::TSTw):	case OpT(Operation::TSTl):
			if constexpr (model == Model::M68000) {
				return ~OneOperandMask<
					AlterableAddressingModesNoAn
				>::value;
			} else {
				return ~OneOperandMask<
					AllModes
				>::value;
			}

		case CMPMb:	case CMPMw:	case CMPMl:
			return ~TwoOperandMask<
				PostInc,
				PostInc
			>::value;

		case OpT(Operation::DBcc):
			return ~TwoOperandMask<
				Dn,
				Imm
			>::value;

		case EXGRtoR:
			return ~TwoOperandMask<
				Dn,
				Dn
			>::value;

		case EXGAtoA:
			return ~TwoOperandMask<
				An,
				An
			>::value;

		case EXGRtoA:
			return ~TwoOperandMask<
				Dn,
				An
			>::value;

		case OpT(Operation::EXTbtow):	case OpT(Operation::EXTwtol):
		case OpT(Operation::SWAP):
			return ~OneOperandMask<
				Dn
			>::value;

		case OpT(Operation::JMP):
		case OpT(Operation::JSR):
		case OpT(Operation::PEA):
			return ~OneOperandMask<
				ControlAddressingModes
			>::value;

		case OpT(Operation::LEA):
			return ~TwoOperandMask<
				ControlAddressingModes,
				An
			>::value;

		case OpT(Operation::LINKw):
			return ~TwoOperandMask<
				An,
				Imm
			>::value;

		case OpT(Operation::NOP):
		case OpT(Operation::RTE):
		case OpT(Operation::RTS):
		case OpT(Operation::TRAPV):
		case OpT(Operation::RTR):
			return ~NoOperandMask::value;

		case OpT(Operation::UNLINK):
		case OpT(Operation::MOVEtoUSP):
		case OpT(Operation::MOVEfromUSP):
			return ~OneOperandMask<
				An
			>::value;

		case MOVEMtoMw:	case MOVEMtoMl:
			return ~TwoOperandMask<
				Imm,
				Ind | PreDec | d16An | d8AnXn | XXXw | XXXl
			>::value;

		case MOVEMtoRw: case MOVEMtoRl:
			return ~TwoOperandMask<
				Ind | PostInc | d16An | d8AnXn | XXXw | XXXl | d16PC | d8PCXn,
				Imm
			>::value;

		case MOVEPtoRl: case MOVEPtoRw:
			return ~TwoOperandMask<
				d16An,
				Dn
			>::value;

		case MOVEPtoMl:	case MOVEPtoMw:
			return ~TwoOperandMask<
				Dn,
				d16An
			>::value;
	}
}

/// Provides a post-decoding validation step — primarily ensures that the prima facie addressing modes are supported by the operation.
template <Model model>
template <uint8_t op, bool validate> Preinstruction Predecoder<model>::validated(
	AddressingMode op1_mode, int op1_reg,
	AddressingMode op2_mode, int op2_reg
) {
	constexpr auto operation = Predecoder<model>::operation(op);

	if constexpr (!validate) {
		return Preinstruction(
			operation,
			op1_mode, op1_reg,
			op2_mode, op2_reg,
			requires_supervisor<model>(operation));
	}

	const auto invalid = invalid_operands<op>();
	const auto observed = operand_mask(op1_mode, op2_mode);
	return (observed & invalid) ?
		Preinstruction() :
		Preinstruction(
			operation,
			op1_mode, op1_reg,
			op2_mode, op2_reg,
			requires_supervisor<model>(operation));
}

/// Decodes the fields within an instruction and constructs a `Preinstruction`, given that the operation has already been
/// decoded. Optionally applies validation
template <Model model>
template <uint8_t op, bool validate> Preinstruction Predecoder<model>::decode(uint16_t instruction) {
	// Fields used pervasively below.
	//
	// Underlying assumption: the compiler will discard whatever of these
	// isn't actually used.
	const auto ea_register = instruction & 7;
	const auto ea_mode = (instruction >> 3) & 7;
	const auto opmode = (instruction >> 6) & 7;
	const auto data_register = (instruction >> 9) & 7;

	switch(op) {

		//
		// MARK: ABCD, SBCD, ADDX.
		//
		// b9–b11:	Rx (destination)
		// b0–b2:	Ry (source)
		// b3:		1 => operation is memory-to-memory; 0 => register-to-register.
		//
		case OpT(Operation::ABCD):	case OpT(Operation::SBCD):
		case OpT(Operation::ADDXb):	case OpT(Operation::ADDXw):	case OpT(Operation::ADDXl):
		case OpT(Operation::SUBXb):	case OpT(Operation::SUBXw):	case OpT(Operation::SUBXl): {
			const auto addressing_mode = (instruction & 8) ?
				AddressingMode::AddressRegisterIndirectWithPredecrement : AddressingMode::DataRegisterDirect;

			return validated<op, validate>(
				addressing_mode, ea_register,
				addressing_mode, data_register);
		}

		//
		// MARK: AND, OR, EOR.
		//
		// b9–b11:			a register;
		// b0–b2 and b3–b5:	an effective address;
		// b6–b8:			an opmode, i.e. source + direction.
		//
		case ADDtoRb:	case ADDtoRw:	case ADDtoRl:
		case SUBtoRb:	case SUBtoRw:	case SUBtoRl:
		case ANDtoRb:	case ANDtoRw:	case ANDtoRl:
		case ORtoRb:	case ORtoRw:	case ORtoRl:
		case OpT(Operation::CMPb):	case OpT(Operation::CMPw):	case OpT(Operation::CMPl):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		case ADDtoMb:	case ADDtoMw:	case ADDtoMl:
		case SUBtoMb:	case SUBtoMw:	case SUBtoMl:
		case ANDtoMb:	case ANDtoMw:	case ANDtoMl:
		case ORtoMb:	case ORtoMw:	case ORtoMl:
		case OpT(Operation::EORb):	case OpT(Operation::EORw):	case OpT(Operation::EORl):
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, data_register,
				combined_mode(ea_mode, ea_register), ea_register);

		case OpT(Operation::ADDAw):	case OpT(Operation::ADDAl):
		case OpT(Operation::SUBAw):	case OpT(Operation::SUBAl):
		case OpT(Operation::CMPAw):	case OpT(Operation::CMPAl):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::AddressRegisterDirect, data_register);

		//
		// MARK: EORI, ORI, ANDI, SUBI, ADDI, CMPI, B[TST/CHG/CLR/SET]I
		//
		// Implicitly:		source is an immediate value;
		// b0–b2 and b3–b5:	destination effective address.
		//
		case EORIb: 	case EORIl:		case EORIw:
		case ORIb:		case ORIl:		case ORIw:
		case ANDIb:		case ANDIl:		case ANDIw:
		case SUBIb:		case SUBIl:		case SUBIw:
		case ADDIb:		case ADDIl:		case ADDIw:
		case CMPIb:		case CMPIl:		case CMPIw:
		case BTSTI:		case BCHGI:
		case BCLRI:		case BSETI:
			return validated<op, validate>(
				AddressingMode::ImmediateData, 0,
				combined_mode(ea_mode, ea_register), ea_register);


		//
		// MARK: BTST, BCLR, BCHG, BSET
		//
		// b0–b2 and b3–b5:	destination effective address;
		// b9–b11:			source data register.
		//
		case OpT(Operation::BTST):	case OpT(Operation::BCLR):
		case OpT(Operation::BCHG):	case OpT(Operation::BSET):
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, data_register,
				combined_mode(ea_mode, ea_register), ea_register);

		//
		// MARK: STOP, ANDItoCCR, ANDItoSR, EORItoCCR, EORItoSR, ORItoCCR, ORItoSR, Bccl, Bccw, BSRl, BSRw
		//
		// Operand is an immedate; destination/source (if any) is implied by the operation.
		//
		case OpT(Operation::STOP):
		case OpT(Operation::Bccl):		case OpT(Operation::Bccw):
		case OpT(Operation::BSRl):		case OpT(Operation::BSRw):
		case OpT(Operation::ORItoSR):	case OpT(Operation::ORItoCCR):
		case OpT(Operation::ANDItoSR):	case OpT(Operation::ANDItoCCR):
		case OpT(Operation::EORItoSR):	case OpT(Operation::EORItoCCR):
			return validated<op, validate>(AddressingMode::ImmediateData);

		//
		// MARK: CHK
		//
		// Implicitly:		destination is a register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case OpT(Operation::CHK):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: EXG.
		//
		// b0–b2:	register Ry (data or address, address if exchange is address <-> data);
		// b9–b11:	register Rx (data or address, data if exchange is address <-> data);
		// b3–b7:	an opmode, indicating address/data registers.
		//
		case EXGRtoR:
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::DataRegisterDirect, ea_register);

		case EXGAtoA:
			return validated<op, validate>(
				AddressingMode::AddressRegisterDirect, data_register,
				AddressingMode::AddressRegisterDirect, ea_register);

		case EXGRtoA:
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::AddressRegisterDirect, ea_register);

		//
		// MARK: MULU, MULS, DIVU, DIVS.
		//
		// b9–b11:			destination data register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case OpT(Operation::DIVU):	case OpT(Operation::DIVS):
		case OpT(Operation::MULU):	case OpT(Operation::MULS):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: LEA
		//
		// b9–b11:			destination address register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case OpT(Operation::LEA):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::AddressRegisterDirect, data_register);

		//
		// MARK: MOVEPtoRw, MOVEPtoRl
		//
		// b0–b2:	an address register;
		// b9–b11:	a data register.
		// [already decoded: b6–b8:	an opmode, indicating size and direction]
		//
		case OpT(MOVEPtoRw):	case OpT(MOVEPtoRl):
			return validated<op, validate>(
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		case OpT(MOVEPtoMw):	case OpT(MOVEPtoMl):
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register);

		//
		// MARK: MOVE
		//
		// b0–b2 and b3–b5:		source effective address;
		// b6–b8 and b9–b11:	destination effective address;
		// [already decoded: b12–b13: size]
		//
		case OpT(Operation::MOVEb):		case OpT(Operation::MOVEl):		case OpT(Operation::MOVEw):
		case OpT(Operation::MOVEAl):	case OpT(Operation::MOVEAw):
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				combined_mode(opmode, data_register), data_register);

		//
		// MARK: RESET, NOP RTE, RTS, TRAPV, RTR
		//
		// No additional fields.
		//
		case OpT(Operation::RESET):	case OpT(Operation::NOP):
		case OpT(Operation::RTE):	case OpT(Operation::RTS):	case OpT(Operation::TRAPV):
		case OpT(Operation::RTR):
			return validated<op, validate>();

		//
		// MARK: NEGX, CLR, NEG, MOVEtoCCR, MOVEtoSR, NOT, NBCD, PEA, TST
		//
		// b0–b2 and b3–b5:		effective address.
		//
		case OpT(Operation::CLRb):		case OpT(Operation::CLRw):		case OpT(Operation::CLRl):
		case OpT(Operation::JMP):		case OpT(Operation::JSR):
		case OpT(Operation::MOVEtoSR):	case OpT(Operation::MOVEfromSR):	case OpT(Operation::MOVEtoCCR):
		case OpT(Operation::NBCD):
		case OpT(Operation::NEGb):		case OpT(Operation::NEGw):		case OpT(Operation::NEGl):
		case OpT(Operation::NEGXb):		case OpT(Operation::NEGXw):		case OpT(Operation::NEGXl):
		case OpT(Operation::NOTb):		case OpT(Operation::NOTw):		case OpT(Operation::NOTl):
		case OpT(Operation::PEA):
		case OpT(Operation::TAS):
		case OpT(Operation::TSTb):		case OpT(Operation::TSTw):		case OpT(Operation::TSTl):
		case OpT(Operation::Scc):
			return validated<op, validate>(combined_mode(ea_mode, ea_register), ea_register);

		//
		// MARK: UNLINK, MOVEtoUSP, MOVEfromUSP
		//
		// b0–b2:		an address register.
		//
		case OpT(Operation::UNLINK):
		case OpT(Operation::MOVEfromUSP):	case OpT(Operation::MOVEtoUSP):
			return validated<op, validate>(AddressingMode::AddressRegisterDirect, ea_register);

		//
		// MARK: DBcc
		//
		// b0–b2:		a data register.
		// Followed by an immediate value.
		//
		case OpT(Operation::DBcc):
			return validated<op, validate>(
				AddressingMode::DataRegisterDirect, ea_register,
				AddressingMode::ImmediateData, 0);

		//
		// MARK: SWAP, EXTbtow, EXTwtol
		//
		// b0–b2:		a data register.
		//
		case OpT(Operation::SWAP):
		case OpT(Operation::EXTbtow):	case OpT(Operation::EXTwtol):
			return validated<op, validate>(AddressingMode::DataRegisterDirect, ea_register);

		//
		// MARK: MOVEMtoMw, MOVEMtoMl, MOVEMtoRw, MOVEMtoRl
		//
		// b0–b2 and b3–b5:		effective address.
		// [already decoded: b10: direction]
		//
		case MOVEMtoMl:	case MOVEMtoMw:
			return validated<op, validate>(
				AddressingMode::ImmediateData, 0,
				combined_mode(ea_mode, ea_register), ea_register);

		case MOVEMtoRl:	case MOVEMtoRw:
			return validated<op, validate>(
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::ImmediateData, 0);

		//
		// MARK: TRAP, BCCb, BSRb
		//
		// No further operands decoded, but note that one is somewhere in the opcode.
		//
		case OpT(Operation::TRAP):
		case OpT(Operation::Bccb):
		case OpT(Operation::BSRb):
			return validated<op, validate>(AddressingMode::Quick);

		//
		// MARK: LINKw
		//
		// b0–b2:		'source' address register;
		// Implicitly:	'destination' is an immediate.
		//
		case OpT(Operation::LINKw):
			return validated<op, validate>(
				AddressingMode::AddressRegisterDirect, ea_register,
				AddressingMode::ImmediateData, 0);

		//
		// MARK: ADDQ, SUBQ
		//
		// b0–b2 and b3–5:	a destination effective address;
		// b9–b11:			an immediate value, embedded in the opcode.
		//
		case ADDQb:		case ADDQw:		case ADDQl:
		case SUBQb:		case SUBQw:		case SUBQl:
			return validated<op, validate>(
				AddressingMode::Quick, 0,
				combined_mode(ea_mode, ea_register), ea_register);

		//
		// MARK: MOVEq
		//
		// b9–b11:		a destination register;
		// b0–b7:		a 'quick' value.
		//
		case MOVEQ:
			return validated<op, validate>(
				AddressingMode::Quick, 0,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: ASR, LSR, ROXR, ROR, ASL, LSL, ROXL, ROL
		//
		// b0–b2:	a register to shift (the source here, for consistency with the memory operations);
		// b8:		0 => b9–b11 are a direct count of bits to shift; 1 => b9–b11 identify a register containing the shift count;
		// b9–b11:	either a quick value or a register.
		case OpT(Operation::ASRb):	case OpT(Operation::ASRw):	case OpT(Operation::ASRl):
		case OpT(Operation::LSRb):	case OpT(Operation::LSRw):	case OpT(Operation::LSRl):
		case OpT(Operation::ROXRb):	case OpT(Operation::ROXRw):	case OpT(Operation::ROXRl):
		case OpT(Operation::RORb):	case OpT(Operation::RORw):	case OpT(Operation::RORl):
		case OpT(Operation::ASLb):	case OpT(Operation::ASLw):	case OpT(Operation::ASLl):
		case OpT(Operation::LSLb):	case OpT(Operation::LSLw):	case OpT(Operation::LSLl):
		case OpT(Operation::ROXLb):	case OpT(Operation::ROXLw):	case OpT(Operation::ROXLl):
		case OpT(Operation::ROLb):	case OpT(Operation::ROLw):	case OpT(Operation::ROLl):
			return validated<op, validate>(
				(instruction & 0x20) ? AddressingMode::DataRegisterDirect : AddressingMode::Quick, data_register,
				AddressingMode::DataRegisterDirect, ea_register);

		//
		// MARK: ASRm, LSRm, ROXRm, RORm, ASLm, LSLm, ROXLm, ROLm
		//
		// b0–b2 and b3–5:	an effective address.
		//
		case OpT(Operation::ASRm):	case OpT(Operation::ASLm):
		case OpT(Operation::LSRm):	case OpT(Operation::LSLm):
		case OpT(Operation::ROXRm):	case OpT(Operation::ROXLm):
		case OpT(Operation::RORm):	case OpT(Operation::ROLm):
			return validated<op, validate>(combined_mode(ea_mode, ea_register), ea_register);

		case CMPMb:	case CMPMw:	case CMPMl:
			return validated<op, validate>(
					AddressingMode::AddressRegisterIndirectWithPostincrement, ea_register,
					AddressingMode::AddressRegisterIndirectWithPostincrement, data_register);

		//
		// MARK: Impossible error case.
		//
		default:
			// Should be unreachable.
			assert(false);
	}
}

// MARK: - Page decoders.

#define Decode(y)	return decode<OpT(y)>(instruction)

template <Model model>
Preinstruction Predecoder<model>::decode0(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0xfff) {
		case 0x03c:	Decode(Op::ORItoCCR);	// 4-155 (p259)
		case 0x07c:	Decode(Op::ORItoSR);	// 6-27 (p481)
		case 0x23c:	Decode(Op::ANDItoCCR);	// 4-20 (p124)
		case 0x27c:	Decode(Op::ANDItoSR);	// 6-2 (p456)
		case 0xa3c:	Decode(Op::EORItoCCR);	// 4-104 (p208)
		case 0xa7c:	Decode(Op::EORItoSR);	// 6-10 (p464)

		default:	break;
	}

	switch(instruction & 0xfc0) {
		// 4-153 (p257)
		case 0x000:	Decode(ORIb);
		case 0x040:	Decode(ORIw);
		case 0x080:	Decode(ORIl);

		// 4-18 (p122)
		case 0x200:	Decode(ANDIb);
		case 0x240:	Decode(ANDIw);
		case 0x280:	Decode(ANDIl);

		// 4-179 (p283)
		case 0x400:	Decode(SUBIb);
		case 0x440:	Decode(SUBIw);
		case 0x480:	Decode(SUBIl);

		// 4-9 (p113)
		case 0x600:	Decode(ADDIb);
		case 0x640:	Decode(ADDIw);
		case 0x680:	Decode(ADDIl);

		// 4-63 (p167)
		case 0x800:	Decode(BTSTI);

		// 4-29 (p133)
		case 0x840:	Decode(BCHGI);

		// 4-32 (p136)
		case 0x880:	Decode(BCLRI);

		// 4-58 (p162)
		case 0x8c0:	Decode(BSETI);

		// 4-102 (p206)
		case 0xa00:	Decode(EORIb);
		case 0xa40:	Decode(EORIw);
		case 0xa80:	Decode(EORIl);

		// 4-79 (p183)
		case 0xc00:	Decode(CMPIb);
		case 0xc40:	Decode(CMPIw);
		case 0xc80:	Decode(CMPIl);

		default:	break;
	}

	switch(instruction & 0x1f8) {
		// 4-133 (p237)
		case 0x108:	Decode(MOVEPtoRw);
		case 0x148:	Decode(MOVEPtoRl);
		case 0x188:	Decode(MOVEPtoMw);
		case 0x1c8:	Decode(MOVEPtoMl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x100:	Decode(Op::BTST);	// 4-62 (p166)
		case 0x180:	Decode(Op::BCLR);	// 4-31 (p135)

		case 0x140:	Decode(Op::BCHG);	// 4-28 (p132)
		case 0x1c0:	Decode(Op::BSET);	// 4-57 (p161)

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode1(uint16_t instruction) {
	using Op = Operation;

	// 4-116 (p220)
	Decode(Op::MOVEb);
}

template <Model model>
Preinstruction Predecoder<model>::decode2(uint16_t instruction) {
	using Op = Operation;

	// 4-116 (p220)
	switch(instruction & 0x1c0) {
		case 0x040:	Decode(Op::MOVEAl);
		default:	Decode(Op::MOVEl);
	}
}

template <Model model>
Preinstruction Predecoder<model>::decode3(uint16_t instruction) {
	using Op = Operation;

	// 4-116 (p220)
	switch(instruction & 0x1c0) {
		case 0x040:	Decode(Op::MOVEAw);
		default:	Decode(Op::MOVEw);
	}
//	Decode(Op::MOVEw);
}

template <Model model>
Preinstruction Predecoder<model>::decode4(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0xfff) {
		case 0xe70:	Decode(Op::RESET);	// 6-83 (p537)
		case 0xe71:	Decode(Op::NOP);	// 4-147 (p251)
		case 0xe72:	Decode(Op::STOP);	// 6-85 (p539)
		case 0xe73:	Decode(Op::RTE);	// 6-84 (p538)
		case 0xe75:	Decode(Op::RTS);	// 4-169 (p273)
		case 0xe76:	Decode(Op::TRAPV);	// 4-191 (p295)
		case 0xe77:	Decode(Op::RTR);	// 4-168 (p272)
		default:	break;
	}

	switch(instruction & 0xff8) {
		case 0x840:	Decode(Op::SWAP);			// 4-185 (p289)
		case 0x880:	Decode(Op::EXTbtow);		// 4-106 (p210)
		case 0x8c0:	Decode(Op::EXTwtol);		// 4-106 (p210)
		case 0xe50:	Decode(Op::LINKw);			// 4-111 (p215)
		case 0xe58:	Decode(Op::UNLINK);			// 4-194 (p298)
		case 0xe60:	Decode(Op::MOVEtoUSP);		// 6-21 (p475)
		case 0xe68:	Decode(Op::MOVEfromUSP);	// 6-21 (p475)
		default:	break;
	}

	switch(instruction & 0xff0) {
		case 0xe40:	Decode(Op::TRAP);		// 4-188 (p292)
		default:	break;
	}

	switch(instruction & 0xfc0) {
		// 4-146 (p250)
		case 0x000:	Decode(Op::NEGXb);
		case 0x040:	Decode(Op::NEGXw);
		case 0x080:	Decode(Op::NEGXl);

		// 6-17 (p471)
		case 0x0c0:	Decode(Op::MOVEfromSR);

		// 4-73 (p177)
		case 0x200:	Decode(Op::CLRb);
		case 0x240:	Decode(Op::CLRw);
		case 0x280:	Decode(Op::CLRl);

		// 4-144 (p247)
		case 0x400:	Decode(Op::NEGb);
		case 0x440:	Decode(Op::NEGw);
		case 0x480:	Decode(Op::NEGl);

		// 4-123 (p227)
		case 0x4c0:	Decode(Op::MOVEtoCCR);

		// 4-148 (p252)
		case 0x600:	Decode(Op::NOTb);
		case 0x640:	Decode(Op::NOTw);
		case 0x680:	Decode(Op::NOTl);

		// 4-123 (p227)
		case 0x6c0:	Decode(Op::MOVEtoSR);

		// 4-142 (p246)
		case 0x800:	Decode(Op::NBCD);

		// 4-159 (p263)
		case 0x840:	Decode(Op::PEA);

		// 4-128 (p232)
		case 0x880:	Decode(MOVEMtoMw);
		case 0x8c0:	Decode(MOVEMtoMl);
		case 0xc80:	Decode(MOVEMtoRw);
		case 0xcc0:	Decode(MOVEMtoRl);

		// 4-192 (p296)
		case 0xa00:	Decode(Op::TSTb);
		case 0xa40:	Decode(Op::TSTw);
		case 0xa80:	Decode(Op::TSTl);

		// 4-186 (p290)
		case 0xac0:	Decode(Op::TAS);

		// 4-109 (p213)
		case 0xe80:	Decode(Op::JSR);

		// 4-108 (p212)
		case 0xec0:	Decode(Op::JMP);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x1c0:	Decode(Op::LEA);	// 4-110 (p214)
		case 0x180:	Decode(Op::CHK);	// 4-69 (p173)
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode5(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x1c0) {
		// 4-11 (p115)
		case 0x000:	Decode(ADDQb);
		case 0x040:	Decode(ADDQw);
		case 0x080:	Decode(ADDQl);

		// 4-181 (p285)
		case 0x100:	Decode(SUBQb);
		case 0x140:	Decode(SUBQw);
		case 0x180:	Decode(SUBQl);

		default:	break;
	}

	switch(instruction & 0x0f8) {
		// 4-173 (p276)
		case 0x0c0:
		case 0x0d0: case 0x0d8:
		case 0x0e0:	case 0x0e8:
		case 0x0f0:	case 0x0f8:	Decode(Op::Scc);

		// 4-91 (p195)
		case 0x0c8:	Decode(Op::DBcc);

		default:	break;
	}
	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode6(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0xf00) {
		// 4-59 (p163)
		case 0x100:
			switch(instruction & 0xff) {
				case 0x00:	Decode(Op::BSRw);
				case 0xff:
					if constexpr (model >= Model::M68020) {
						Decode(Op::BSRl);
					}
					[[fallthrough]];
				default:	Decode(Op::BSRb);
			}

		// 4-25 (p129) Bcc
		// 4-55 (p159) BRA (i.e. Bcc with cc = always)
		default:
			switch(instruction & 0xff) {
				case 0x00:	Decode(Op::Bccw);
				case 0xff:
					if constexpr (model >= Model::M68020) {
						Decode(Op::Bccl);
					}
					[[fallthrough]];
				default:	Decode(Op::Bccb);
			}
	}
}

template <Model model>
Preinstruction Predecoder<model>::decode7(uint16_t instruction) {
	using Op = Operation;

	// 4-134 (p238)
	if(!(instruction & 0x100)) {
		Decode(MOVEQ);
	} else {
		return Preinstruction();
	}
}

template <Model model>
Preinstruction Predecoder<model>::decode8(uint16_t instruction) {
	using Op = Operation;

	// 4-171 (p275)
	if((instruction & 0x1f0) == 0x100) Decode(Op::SBCD);

	switch(instruction & 0x1c0) {
		case 0x0c0:	Decode(Op::DIVU);	// 4-97 (p201)
		case 0x1c0:	Decode(Op::DIVS);	// 4-93 (p197)

		// 4-150 (p254)
		case 0x000:	Decode(ORtoRb);
		case 0x040:	Decode(ORtoRw);
		case 0x080:	Decode(ORtoRl);
		case 0x100:	Decode(ORtoMb);
		case 0x140:	Decode(ORtoMw);
		case 0x180:	Decode(ORtoMl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode9(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x1f0) {
		// 4-184 (p288)
		case 0x100:	Decode(Op::SUBXb);
		case 0x140:	Decode(Op::SUBXw);
		case 0x180:	Decode(Op::SUBXl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-177 (p281)
		case 0x0c0:	Decode(Op::SUBAw);
		case 0x1c0:	Decode(Op::SUBAl);

		// 4-174 (p278)
		case 0x000:	Decode(SUBtoRb);
		case 0x040:	Decode(SUBtoRw);
		case 0x080:	Decode(SUBtoRl);
		case 0x100:	Decode(SUBtoMb);
		case 0x140:	Decode(SUBtoMw);
		case 0x180:	Decode(SUBtoMl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeA(uint16_t) {
	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeB(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x1f8) {
		// 4-81 (p185)
		case 0x108:	Decode(CMPMb);
		case 0x148:	Decode(CMPMw);
		case 0x188:	Decode(CMPMl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-75 (p179)
		case 0x000:	Decode(Op::CMPb);
		case 0x040:	Decode(Op::CMPw);
		case 0x080:	Decode(Op::CMPl);

		// 4-77 (p181)
		case 0x0c0:	Decode(Op::CMPAw);
		case 0x1c0:	Decode(Op::CMPAl);

		// 4-100 (p204)
		case 0x100:	Decode(Op::EORb);
		case 0x140:	Decode(Op::EORw);
		case 0x180:	Decode(Op::EORl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeC(uint16_t instruction) {
	using Op = Operation;

	// 4-105 (p209)
	switch(instruction & 0x1f8) {
		case 0x140:	Decode(EXGRtoR);
		case 0x148:	Decode(EXGAtoA);
		case 0x188:	Decode(EXGRtoA);
		default:	break;
	}

	switch(instruction & 0x1f0) {
		case 0x100:	Decode(Op::ABCD);	// 4-3 (p107)
		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	Decode(Op::MULU);	// 4-139 (p243)
		case 0x1c0:	Decode(Op::MULS);	// 4-136 (p240)

		// 4-15 (p119)
		case 0x000:	Decode(ANDtoRb);
		case 0x040:	Decode(ANDtoRw);
		case 0x080:	Decode(ANDtoRl);
		case 0x100:	Decode(ANDtoMb);
		case 0x140:	Decode(ANDtoMw);
		case 0x180:	Decode(ANDtoMl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeD(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x1f0) {
		// 4-14 (p118)
		case 0x100:	Decode(Op::ADDXb);
		case 0x140:	Decode(Op::ADDXw);
		case 0x180:	Decode(Op::ADDXl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-7 (p111)
		case 0x0c0:	Decode(Op::ADDAw);
		case 0x1c0:	Decode(Op::ADDAl);

		// 4-4 (p108)
		case 0x000:	Decode(ADDtoRb);
		case 0x040:	Decode(ADDtoRw);
		case 0x080:	Decode(ADDtoRl);
		case 0x100:	Decode(ADDtoMb);
		case 0x140:	Decode(ADDtoMw);
		case 0x180:	Decode(ADDtoMl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeE(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0xfc0) {
		case 0x0c0:	Decode(Op::ASRm);	// 4-22 (p126)
		case 0x1c0:	Decode(Op::ASLm);	// 4-22 (p126)
		case 0x2c0:	Decode(Op::LSRm);	// 4-113 (p217)
		case 0x3c0:	Decode(Op::LSLm);	// 4-113 (p217)
		case 0x4c0:	Decode(Op::ROXRm);	// 4-163 (p267)
		case 0x5c0:	Decode(Op::ROXLm);	// 4-163 (p267)
		case 0x6c0:	Decode(Op::RORm);	// 4-160 (p264)
		case 0x7c0:	Decode(Op::ROLm);	// 4-160 (p264)

		default:	break;
	}

	switch(instruction & 0x1d8) {
		// 4-22 (p126)
		case 0x000:	Decode(Op::ASRb);
		case 0x040:	Decode(Op::ASRw);
		case 0x080:	Decode(Op::ASRl);

		// 4-113 (p217)
		case 0x008:	Decode(Op::LSRb);
		case 0x048:	Decode(Op::LSRw);
		case 0x088:	Decode(Op::LSRl);

		// 4-163 (p267)
		case 0x010:	Decode(Op::ROXRb);
		case 0x050:	Decode(Op::ROXRw);
		case 0x090:	Decode(Op::ROXRl);

		// 4-160 (p264)
		case 0x018:	Decode(Op::RORb);
		case 0x058:	Decode(Op::RORw);
		case 0x098:	Decode(Op::RORl);

		// 4-22 (p126)
		case 0x100:	Decode(Op::ASLb);
		case 0x140:	Decode(Op::ASLw);
		case 0x180:	Decode(Op::ASLl);

		// 4-113 (p217)
		case 0x108:	Decode(Op::LSLb);
		case 0x148:	Decode(Op::LSLw);
		case 0x188:	Decode(Op::LSLl);

		// 4-163 (p267)
		case 0x110:	Decode(Op::ROXLb);
		case 0x150:	Decode(Op::ROXLw);
		case 0x190:	Decode(Op::ROXLl);

		// 4-160 (p264)
		case 0x118:	Decode(Op::ROLb);
		case 0x158:	Decode(Op::ROLw);
		case 0x198:	Decode(Op::ROLl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeF(uint16_t) {
	return Preinstruction();
}

#undef Decode

// MARK: - Main decoder.

template <Model model>
Preinstruction Predecoder<model>::decode(uint16_t instruction) {
	// Divide first based on line.
	switch(instruction & 0xf000) {
		case 0x0000:	return decode0(instruction);
		case 0x1000:	return decode1(instruction);
		case 0x2000:	return decode2(instruction);
		case 0x3000:	return decode3(instruction);
		case 0x4000:	return decode4(instruction);
		case 0x5000:	return decode5(instruction);
		case 0x6000:	return decode6(instruction);
		case 0x7000:	return decode7(instruction);
		case 0x8000:	return decode8(instruction);
		case 0x9000:	return decode9(instruction);
		case 0xa000:	return decodeA(instruction);
		case 0xb000:	return decodeB(instruction);
		case 0xc000:	return decodeC(instruction);
		case 0xd000:	return decodeD(instruction);
		case 0xe000:	return decodeE(instruction);
		case 0xf000:	return decodeF(instruction);

		default:	break;
	}

	return Preinstruction();
}

template class InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000>;
template class InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68010>;
template class InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68020>;
template class InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68030>;
template class InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68040>;
