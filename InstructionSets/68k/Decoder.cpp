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

/// @returns The @c AddressingMode given the specified mode and reg, subject to potential
/// 	aliasing on the '020+ as described above the @c AddressingMode enum.
template <
	bool allow_An = true, bool allow_post_inc = true
> constexpr AddressingMode combined_mode(int raw_mode, int reg) {
	auto mode = AddressingMode(raw_mode);

	if(!allow_An && mode == AddressingMode::AddressRegisterDirect) {
		mode = AddressingMode::DataRegisterDirect;
	}
	if(!allow_post_inc && mode == AddressingMode::AddressRegisterIndirectWithPostincrement) {
		mode = AddressingMode::AddressRegisterIndirect;
	}

	return (raw_mode != 7) ? mode : AddressingMode(0b01'000 | reg);
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
constexpr Operation Predecoder<model>::operation(Op op) {
	if(op < Op(Operation::Max)) {
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
		case SUBQw:		return Operation::SUBl;
		case SUBQAw:	return Operation::SUBAw;
		case SUBQAl:	return Operation::SUBAl;

		case BTSTI:		return Operation::BTST;
		case BCHGI:		return Operation::BCHG;
		case BCLRI:		return Operation::BCLR;
		case BSETI:		return Operation::BSET;

		default: break;
	}

	return Operation::Undefined;
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

	constexpr auto operation = Predecoder<model>::operation(op);

	switch(op) {

		//
		// MARK: ABCD, SBCD.
		//
		case Op(Operation::ABCD):	case Op(Operation::SBCD):	{
			const auto addressing_mode = (instruction & 8) ?
				AddressingMode::AddressRegisterIndirectWithPredecrement : AddressingMode::DataRegisterDirect;

			return Preinstruction(operation,
				addressing_mode, ea_register,
				addressing_mode, data_register);
		}

		//
		// MARK: AND, OR, EOR.
		//
		case Op(Operation::ANDb):	case Op(Operation::ANDw):	case Op(Operation::ANDl):
		case Op(Operation::ORb):	case Op(Operation::ORw):	case Op(Operation::ORl):
		case Op(Operation::EORb):	case Op(Operation::EORw):	case Op(Operation::EORl):	{
			// Opmode 7 is illegal.
			if(opmode == 7) {
				return Preinstruction();
			}

			constexpr bool is_eor =
				operation == Operation::EORb ||
				operation == Operation::EORw ||
				operation == Operation::EORl;

			const auto ea_combined_mode = combined_mode(ea_mode, ea_register);

			if(opmode & 4) {
				// Dn Λ < ea > → < ea >

				// The operations other than EOR do not permit <ea>
				// to be a data register; targetting a data register
				// should be achieved with the alternative opmode.
				if constexpr (!is_eor) {
					if(ea_combined_mode == AddressingMode::DataRegisterDirect) {
						return Preinstruction();
					}
				}

				return Preinstruction(operation,
					AddressingMode::DataRegisterDirect, data_register,
					ea_combined_mode, ea_register);
			} else {
				// < ea > Λ Dn → Dn

				// EOR doesn't permit → Dn.
				if constexpr (is_eor) {
					return Preinstruction();
				}

				return Preinstruction(operation,
					ea_combined_mode, ea_register,
					AddressingMode::DataRegisterDirect, data_register);
			}

			return Preinstruction();
		}

		//
		// MARK: EORI, ORI, ANDI, SUBI, ADDI, CMPI, B[TST/CHG/CLR/SET]I
		//
		case EORIb: 	case EORIl:		case EORIw:
		case ORIb:		case ORIl:		case ORIw:
		case ANDIb:		case ANDIl:		case ANDIw:
		case SUBIb:		case SUBIl:		case SUBIw:
		case ADDIb:		case ADDIl:		case ADDIw:
		case CMPIb:		case CMPIl:		case CMPIw:
		case BTSTI:		case BCHGI:
		case BCLRI:		case BSETI:
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				combined_mode(ea_mode, ea_register), ea_register);


		//
		// MARK: BTST, BCLR, BCHG, BSET
		//
		case Op(Operation::BTST):	case Op(Operation::BCLR):
		case Op(Operation::BCHG):	case Op(Operation::BSET):
			return Preinstruction(operation,
				AddressingMode::DataRegisterDirect, data_register,
				combined_mode(ea_mode, ea_register), ea_register);

		//
		// MARK: ANDItoCCR, ANDItoSR, EORItoCCR, EORItoSR, ORItoCCR, ORItoSR
		//
		case Op(Operation::ORItoSR):	case Op(Operation::ORItoCCR):
		case Op(Operation::ANDItoSR):	case Op(Operation::ANDItoCCR):
		case Op(Operation::EORItoSR):	case Op(Operation::EORItoCCR):
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				operation == Operation::ORItoSR || operation == Operation::ANDItoSR || operation == Operation::EORItoSR);

		//
		// MARK: EXG.
		//
		case Op(Operation::EXG):
			switch((instruction >> 3)&31) {
				default:	return Preinstruction();

				case 0x08:	return Preinstruction(operation,
					AddressingMode::DataRegisterDirect, ea_register,
					AddressingMode::DataRegisterDirect, data_register);

				case 0x09:	return Preinstruction(operation,
					AddressingMode::AddressRegisterDirect, ea_register,
					AddressingMode::AddressRegisterDirect, data_register);

				case 0x11:	return Preinstruction(operation,
					AddressingMode::AddressRegisterDirect, ea_register,
					AddressingMode::DataRegisterDirect, data_register);
			}

		//
		// MARK: MULU, MULS, DIVU, DIVS.
		//
		case Op(Operation::DIVU):	case Op(Operation::DIVS):
		case Op(Operation::MULU):	case Op(Operation::MULS):
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: MOVEPtoRw, MOVEPtoRl
		//
		case Op(MOVEPtoRw):	case Op(MOVEPtoRl):
			return Preinstruction(operation,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		case Op(MOVEPtoMw):	case Op(MOVEPtoMl):
			return Preinstruction(operation,
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register);

		//
		// MARK: MOVE
		//
		case Op(Operation::MOVEb):	case Op(Operation::MOVEl):	case Op(Operation::MOVEw):
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				combined_mode<false, false>(opmode, data_register), data_register);

		//
		// MARK: STOP, RESET, NOP RTE, RTS, TRAPV, RTR
		//
		case Op(Operation::STOP):	case Op(Operation::RESET):	case Op(Operation::NOP):
		case Op(Operation::RTE):	case Op(Operation::RTS):	case Op(Operation::TRAPV):
		case Op(Operation::RTR):
			return Preinstruction(operation);

		//
		// MARK: NEGX, CLR, NEG, MOVEtoCCR, MOVEtoSR, NOT, NBCD, PEA, TST
		//
		case Op(Operation::CLRb):		case Op(Operation::CLRw):		case Op(Operation::CLRl):
		case Op(Operation::JMP):		case Op(Operation::JSR):
		case Op(Operation::MOVEtoSR):	case Op(Operation::MOVEfromSR):	case Op(Operation::MOVEtoCCR):
		case Op(Operation::NBCD):
		case Op(Operation::NEGb):		case Op(Operation::NEGw):		case Op(Operation::NEGl):
		case Op(Operation::NEGXb):		case Op(Operation::NEGXw):		case Op(Operation::NEGXl):
		case Op(Operation::NOTb):		case Op(Operation::NOTw):		case Op(Operation::NOTl):
		case Op(Operation::PEA):
		case Op(Operation::TAS):
		case Op(Operation::TSTb):		case Op(Operation::TSTw):		case Op(Operation::TSTl):
			return Preinstruction(operation,
				combined_mode<false, false>(ea_mode, ea_register), ea_register);

		//
		// MARK: MOVEMtoMw, MOVEMtoMl, MOVEMtoRw, MOVEMtoRl
		//
		case MOVEMtoMl:	case MOVEMtoMw:
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				combined_mode(ea_mode, ea_register), ea_register);

		case MOVEMtoRl:	case MOVEMtoRw:
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::ImmediateData, 0);

		// TODO: more validation on the above.

		//
		// MARK: Impossible error case.
		//
		default:
			// Should be unreachable.
			assert(false);
	}

	// TODO: be willing to mutate Scc into DBcc.
}

#undef Op

// MARK: - Page decoders.

#define DecodeOp(y)		return decode<Op(Operation::y)>(instruction)
#define DecodeEop(y)	return decode<Op(y)>(instruction)

template <Model model>
Preinstruction Predecoder<model>::decode0(uint16_t instruction) {
	switch(instruction & 0xfff) {
		case 0x03c:	DecodeOp(ORItoCCR);		// 4-155 (p259)
		case 0x07c:	DecodeOp(ORItoSR);		// 6-27 (p481)
		case 0x23c:	DecodeOp(ANDItoCCR);	// 4-20 (p124)
		case 0x27c:	DecodeOp(ANDItoSR);		// 6-2 (p456)
		case 0xa3c:	DecodeOp(EORItoCCR);	// 4-104 (p208)
		case 0xa7c:	DecodeOp(EORItoSR);		// 6-10 (p464)

		default:	break;
	}

	switch(instruction & 0xfc0) {
		// 4-153 (p257)
		case 0x000:	DecodeEop(ORIb);
		case 0x040:	DecodeEop(ORIw);
		case 0x080:	DecodeEop(ORIl);

		// 4-18 (p122)
		case 0x200:	DecodeEop(ANDIb);
		case 0x240:	DecodeEop(ANDIw);
		case 0x280:	DecodeEop(ANDIl);

		// 4-179 (p283)
		case 0x400:	DecodeEop(SUBIb);
		case 0x440:	DecodeEop(SUBIw);
		case 0x480:	DecodeEop(SUBIl);

		// 4-9 (p113)
		case 0x600:	DecodeEop(ADDIb);
		case 0x640:	DecodeEop(ADDIw);
		case 0x680:	DecodeEop(ADDIl);

		// 4-63 (p167)
		case 0x800:	DecodeEop(BTSTI);

		// 4-29 (p133)
		case 0x840:	DecodeEop(BCHGI);

		// 4-32 (p136)
		case 0x880:	DecodeEop(BCLRI);

		// 4-58 (p162)
		case 0x8c0:	DecodeEop(BSETI);

		// 4-102 (p206)
		case 0xa00:	DecodeEop(EORIb);
		case 0xa40:	DecodeEop(EORIw);
		case 0xa80:	DecodeEop(EORIl);

		// 4-79 (p183)
		case 0xc00:	DecodeEop(CMPIb);
		case 0xc40:	DecodeEop(CMPIw);
		case 0xc80:	DecodeEop(CMPIl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x100:	DecodeOp(BTST);	// 4-62 (p166)
		case 0x180:	DecodeOp(BCLR);	// 4-31 (p135)

		case 0x140:	DecodeOp(BCHG);	// 4-28 (p132)
		case 0x1c0:	DecodeOp(BSET);	// 4-57 (p161)

		default:	break;
	}

	switch(instruction & 0x1f8) {
		// 4-133 (p237)
		case 0x108:	DecodeEop(MOVEPtoRw);
		case 0x148:	DecodeEop(MOVEPtoRl);
		case 0x188:	DecodeEop(MOVEPtoMw);
		case 0x1c8:	DecodeEop(MOVEPtoMl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode1(uint16_t instruction) {
	// 4-116 (p220)
	DecodeOp(MOVEb);
}


template <Model model>
Preinstruction Predecoder<model>::decode2(uint16_t instruction) {
	// 4-116 (p220)
	DecodeOp(MOVEl);
}

template <Model model>
Preinstruction Predecoder<model>::decode3(uint16_t instruction) {
	// 4-116 (p220)
	DecodeOp(MOVEw);
}

template <Model model>
Preinstruction Predecoder<model>::decode4(uint16_t instruction) {
	switch(instruction & 0xfff) {
		case 0xe70:	DecodeOp(RESET);	// 6-83 (p537)
		case 0xe71:	DecodeOp(NOP);		// 4-147 (p251)
		case 0xe73:	DecodeOp(RTE);		// 6-84 (p538)
		case 0xe75:	DecodeOp(RTS);		// 4-169 (p273)
		case 0xe76:	DecodeOp(TRAPV);	// 4-191 (p295)
		case 0xe77:	DecodeOp(RTR);		// 4-168 (p272)
		default:	break;
	}

	switch(instruction & 0xfc0) {
		// 4-146 (p250)
		case 0x000:	DecodeOp(NEGXb);
		case 0x040:	DecodeOp(NEGXw);
		case 0x080:	DecodeOp(NEGXl);

		// 6-17 (p471)
		case 0x0c0:	DecodeOp(MOVEfromSR);

		// 4-73 (p177)
		case 0x200:	DecodeOp(CLRb);
		case 0x240:	DecodeOp(CLRw);
		case 0x280:	DecodeOp(CLRl);

		// 4-144 (p247)
		case 0x400:	DecodeOp(NEGb);
		case 0x440:	DecodeOp(NEGw);
		case 0x480:	DecodeOp(NEGl);

		// 4-123 (p227)
		case 0x4c0:	DecodeOp(MOVEtoCCR);

		// 4-148 (p252)
		case 0x600:	DecodeOp(NOTb);
		case 0x640:	DecodeOp(NOTw);
		case 0x680:	DecodeOp(NOTl);

		// 4-123 (p227)
		case 0x6c0:	DecodeOp(MOVEtoSR);

		// 4-142 (p246)
		case 0x800:	DecodeOp(NBCD);

		// 4-159 (p263)
		case 0x840:	DecodeOp(PEA);

		// 4-128 (p232)
		case 0x880:	DecodeEop(MOVEMtoMw);
		case 0x8c0:	DecodeEop(MOVEMtoMl);
		case 0xc80:	DecodeEop(MOVEMtoRw);
		case 0xcc0:	DecodeEop(MOVEMtoRl);

		// 4-192 (p296)
		case 0xa00:	DecodeOp(TSTb);
		case 0xa40:	DecodeOp(TSTw);
		case 0xa80:	DecodeOp(TSTl);

		// 4-186 (p290)
		case 0xac0:	DecodeOp(TAS);

		// 4-109 (p213)
		case 0xe80:	DecodeOp(JSR);

		// 4-108 (p212)
		case 0xec0:	DecodeOp(JMP);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x1c0:	DecodeOp(MOVEAl);	// 4-110 (p214)		TODO: In this I assume that LEA is just a special MOVEAl. Consider.
		case 0x180:	DecodeOp(CHK);		// 4-69 (p173)
		default:	break;
	}

	switch(instruction & 0xff0) {
		case 0xe40:	DecodeOp(TRAP);		// 4-188 (p292)
		default:	break;
	}

	switch(instruction & 0xff8) {
		case 0x860:	DecodeOp(SWAP);			// 4-185 (p289)
		case 0x880:	DecodeOp(EXTbtow);		// 4-106 (p210)
		case 0x8c0:	DecodeOp(EXTwtol);		// 4-106 (p210)
		case 0xe50:	DecodeOp(LINKw);		// 4-111 (p215)
		case 0xe58:	DecodeOp(UNLINK);		// 4-194 (p298)
		case 0xe60:	DecodeOp(MOVEtoUSP);	// 6-21 (p475)
		case 0xe68:	DecodeOp(MOVEfromUSP);	// 6-21 (p475)
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode5(uint16_t instruction) {
	switch(instruction & 0x1c0) {
		// 4-11 (p115)
		case 0x000:	DecodeEop(ADDQb);
		case 0x040:	DecodeEop(ADDQw);
		case 0x080:	DecodeEop(ADDQl);

		// 4-181 (p285)
		case 0x100:	DecodeEop(SUBQb);
		case 0x140:	DecodeEop(SUBQw);
		case 0x180:	DecodeEop(SUBQl);

		default:	break;
	}

	switch(instruction & 0x0c0) {
		// 4-173 (p276), though this'll also hit DBcc 4-91 (p195)
		case 0x0c0:	DecodeOp(Scc);

		default:	break;
	}
	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode6(uint16_t instruction) {
	// 4-25 (p129), 4-59 (p163) and 4-55 (p159)
	DecodeOp(Bcc);
}

template <Model model>
Preinstruction Predecoder<model>::decode7(uint16_t instruction) {
	// 4-134 (p238)
	DecodeOp(MOVEq);
}

template <Model model>
Preinstruction Predecoder<model>::decode8(uint16_t instruction) {
	// 4-171 (p275)
	if((instruction & 0x1f0) == 0x100) DecodeOp(SBCD);

	// 4-150 (p254)
	switch(instruction & 0x0c0) {
		case 0x00:	DecodeOp(ORb);
		case 0x40:	DecodeOp(ORw);
		case 0x80:	DecodeOp(ORl);
		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	DecodeOp(DIVU);	// 4-97 (p201)
		case 0x1c0:	DecodeOp(DIVS);	// 4-93 (p197)
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode9(uint16_t instruction) {
	switch(instruction & 0x0c0) {
		// 4-174 (p278)
		case 0x00:	DecodeOp(SUBb);
		case 0x40:	DecodeOp(SUBw);
		case 0x80:	DecodeOp(SUBl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-177 (p281)
		case 0x0c0:	DecodeOp(SUBAw);
		case 0x1c0:	DecodeOp(SUBAl);

		default:	break;
	}

	switch(instruction & 0x1f0) {
		// 4-184 (p288)
		case 0x100:	DecodeOp(SUBXb);
		case 0x140:	DecodeOp(SUBXw);
		case 0x180:	DecodeOp(SUBXl);

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
	switch(instruction & 0x0c0) {
		// 4-100 (p204)
		case 0x000:	DecodeOp(EORb);
		case 0x040:	DecodeOp(EORw);
		case 0x080:	DecodeOp(EORl);
		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-75 (p179)
		case 0x000:	DecodeOp(CMPb);
		case 0x040:	DecodeOp(CMPw);
		case 0x080:	DecodeOp(CMPl);

		// 4-77 (p181)
		case 0x0c0:	DecodeOp(CMPAw);
		case 0x1c0:	DecodeOp(CMPAl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeC(uint16_t instruction) {
	switch(instruction & 0x1f0) {
		case 0x100:	DecodeOp(ABCD);	// 4-3 (p107)
		default:	break;
	}

	switch(instruction & 0x0c0) {
		// 4-15 (p119)
		case 0x00:	DecodeOp(ANDb);
		case 0x40:	DecodeOp(ANDw);
		case 0x80:	DecodeOp(ANDl);
		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	DecodeOp(MULU);	// 4-139 (p243)
		case 0x1c0:	DecodeOp(MULS);	// 4-136 (p240)
		default:	break;
	}

	// 4-105 (p209)
	switch(instruction & 0x1f8) {
		case 0x140:
		case 0x148:
		case 0x188:	DecodeOp(EXG);
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeD(uint16_t instruction) {
	switch(instruction & 0x0c0) {
		// 4-4 (p108)
		case 0x000:	DecodeOp(ADDb);
		case 0x040:	DecodeOp(ADDw);
		case 0x080:	DecodeOp(ADDl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-7 (p111)
		case 0x0c0:	DecodeOp(ADDAw);
		case 0x1c0:	DecodeOp(ADDAl);

		default:	break;
	}

	switch(instruction & 0x1f0) {
		// 4-14 (p118)
		case 0x100:	DecodeOp(ADDXb);
		case 0x140:	DecodeOp(ADDXw);
		case 0x180:	DecodeOp(ADDXl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeE(uint16_t instruction) {
	switch(instruction & 0x1d8) {
		// 4-22 (p126)
		case 0x000:	DecodeOp(ASRb);
		case 0x040:	DecodeOp(ASRw);
		case 0x080:	DecodeOp(ASRl);

		// 4-113 (p217)
		case 0x008:	DecodeOp(LSRb);
		case 0x048:	DecodeOp(LSRw);
		case 0x088:	DecodeOp(LSRl);

		// 4-163 (p267)
		case 0x010:	DecodeOp(ROXRb);
		case 0x050:	DecodeOp(ROXRw);
		case 0x090:	DecodeOp(ROXRl);

		// 4-160 (p264)
		case 0x018:	DecodeOp(RORb);
		case 0x058:	DecodeOp(RORw);
		case 0x098:	DecodeOp(RORl);

		// 4-22 (p126)
		case 0x100:	DecodeOp(ASLb);
		case 0x140:	DecodeOp(ASLw);
		case 0x180:	DecodeOp(ASLl);

		// 4-113 (p217)
		case 0x108:	DecodeOp(LSLb);
		case 0x148:	DecodeOp(LSLw);
		case 0x188:	DecodeOp(LSLl);

		// 4-163 (p267)
		case 0x110:	DecodeOp(ROXLb);
		case 0x150:	DecodeOp(ROXLw);
		case 0x190:	DecodeOp(ROXLl);

		// 4-160 (p264)
		case 0x118:	DecodeOp(ROLb);
		case 0x158:	DecodeOp(ROLw);
		case 0x198:	DecodeOp(ROLl);

		default:	break;
	}

	switch(instruction & 0xfc0) {
		case 0x0c0:	DecodeOp(ASRm);		// 4-22 (p126)
		case 0x1c0:	DecodeOp(ASLm);		// 4-22 (p126)
		case 0x2c0:	DecodeOp(LSRm);		// 4-113 (p217)
		case 0x3c0:	DecodeOp(LSLm);		// 4-113 (p217)
		case 0x4c0:	DecodeOp(ROXRm);	// 4-163 (p267)
		case 0x5c0:	DecodeOp(ROXLm);	// 4-163 (p267)
		case 0x6c0:	DecodeOp(RORm);		// 4-160 (p264)
		case 0x7c0:	DecodeOp(ROLm);		// 4-160 (p264)

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeF(uint16_t) {
	return Preinstruction();
}

#undef DecodeOp
#undef DecodeEop

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
