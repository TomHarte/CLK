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
		case SUBQw:		return Operation::SUBl;
		case SUBQAw:	return Operation::SUBAw;
		case SUBQAl:	return Operation::SUBAl;

		case BTSTI:		return Operation::BTST;
		case BCHGI:		return Operation::BCHG;
		case BCLRI:		return Operation::BCLR;
		case BSETI:		return Operation::BSET;

		case LEA:		return Operation::MOVEAl;

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
		// b9–b11:	Rx (destination)
		// b0–b2:	Ry (source)
		// b3:		1 => operation is memory-to-memory; 0 => register-to-register.
		//
		case OpT(Operation::ABCD):	case OpT(Operation::SBCD):	{
			const auto addressing_mode = (instruction & 8) ?
				AddressingMode::AddressRegisterIndirectWithPredecrement : AddressingMode::DataRegisterDirect;

			return Preinstruction(operation,
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
		case OpT(Operation::ANDb):	case OpT(Operation::ANDw):	case OpT(Operation::ANDl):
		case OpT(Operation::ORb):	case OpT(Operation::ORw):	case OpT(Operation::ORl):
		case OpT(Operation::EORb):	case OpT(Operation::EORw):	case OpT(Operation::EORl):	{
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
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				combined_mode(ea_mode, ea_register), ea_register);


		//
		// MARK: BTST, BCLR, BCHG, BSET
		//
		// Implicitly:		source is a register;
		// b0–b2 and b3–b5:	destination effective address.
		//
		case OpT(Operation::BTST):	case OpT(Operation::BCLR):
		case OpT(Operation::BCHG):	case OpT(Operation::BSET):
			return Preinstruction(operation,
				AddressingMode::DataRegisterDirect, data_register,
				combined_mode(ea_mode, ea_register), ea_register);

		//
		// MARK: ANDItoCCR, ANDItoSR, EORItoCCR, EORItoSR, ORItoCCR, ORItoSR
		//
		// Operand is an immedate; destination/source is implied by the operation.
		//
		case OpT(Operation::ORItoSR):	case OpT(Operation::ORItoCCR):
		case OpT(Operation::ANDItoSR):	case OpT(Operation::ANDItoCCR):
		case OpT(Operation::EORItoSR):	case OpT(Operation::EORItoCCR):
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				operation == Operation::ORItoSR || operation == Operation::ANDItoSR || operation == Operation::EORItoSR);

		//
		// MARK: CHK
		//
		// Implicitly:		destination is a register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case OpT(Operation::CHK):
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: EXG.
		//
		// b0–b2:	register Ry (data or address, address if exchange is address <-> data);
		// b9–b11:	register Rx (data or address, data if exchange is address <-> data);
		// b3–b7:	an opmode, indicating address/data registers.
		//
		case OpT(Operation::EXG):
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
		// b9–b11:			destination data register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case OpT(Operation::DIVU):	case OpT(Operation::DIVS):
		case OpT(Operation::MULU):	case OpT(Operation::MULS):
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: LEA
		//
		// b9–b11:			destination address register;
		// b0–b2 and b3–b5:	source effective address.
		//
		case LEA:
			return Preinstruction(operation,
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
			return Preinstruction(operation,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		case OpT(MOVEPtoMw):	case OpT(MOVEPtoMl):
			return Preinstruction(operation,
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register);

		//
		// MARK: MOVE
		//
		// b0–b2 and b3–b5:		source effective address;
		// b6–b8 and b9–b11:	destination effective address;
		// [already decoded: b12–b13: size]
		//
		case OpT(Operation::MOVEb):	case OpT(Operation::MOVEl):	case OpT(Operation::MOVEw):
			return Preinstruction(operation,
				combined_mode(ea_mode, ea_register), ea_register,
				combined_mode<false, false>(opmode, data_register), data_register);

		//
		// MARK: STOP, RESET, NOP RTE, RTS, TRAPV, RTR
		//
		// No additional fields.
		//
		case OpT(Operation::STOP):	case OpT(Operation::RESET):	case OpT(Operation::NOP):
		case OpT(Operation::RTE):	case OpT(Operation::RTS):	case OpT(Operation::TRAPV):
		case OpT(Operation::RTR):
			return Preinstruction(operation);

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
			return Preinstruction(operation,
				combined_mode<false, false>(ea_mode, ea_register), ea_register);

		//
		// MARK: MOVEMtoMw, MOVEMtoMl, MOVEMtoRw, MOVEMtoRl
		//
		// b0–b2 and b3–b5:		effective address.
		// [already decoded: b10: direction]
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
		// MARK: TRAP
		//
		// No further operands decoded, but note that one is somewhere in the opcode.
		//
		case OpT(Operation::TRAP):
			return Preinstruction(operation,
				AddressingMode::Quick, 0);

		//
		// MARK: Impossible error case.
		//
		default:
			// Should be unreachable.
			assert(false);
	}

	// TODO: be willing to mutate Scc into DBcc.
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

	switch(instruction & 0x1c0) {
		case 0x100:	Decode(Op::BTST);	// 4-62 (p166)
		case 0x180:	Decode(Op::BCLR);	// 4-31 (p135)

		case 0x140:	Decode(Op::BCHG);	// 4-28 (p132)
		case 0x1c0:	Decode(Op::BSET);	// 4-57 (p161)

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
	Decode(Op::MOVEl);
}

template <Model model>
Preinstruction Predecoder<model>::decode3(uint16_t instruction) {
	using Op = Operation;

	// 4-116 (p220)
	Decode(Op::MOVEw);
}

template <Model model>
Preinstruction Predecoder<model>::decode4(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0xfff) {
		case 0xe70:	Decode(Op::RESET);	// 6-83 (p537)
		case 0xe71:	Decode(Op::NOP);	// 4-147 (p251)
		case 0xe73:	Decode(Op::RTE);	// 6-84 (p538)
		case 0xe75:	Decode(Op::RTS);	// 4-169 (p273)
		case 0xe76:	Decode(Op::TRAPV);	// 4-191 (p295)
		case 0xe77:	Decode(Op::RTR);	// 4-168 (p272)
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
		case 0x1c0:	Decode(LEA);		// 4-110 (p214)
		case 0x180:	Decode(Op::CHK);	// 4-69 (p173)
		default:	break;
	}

	switch(instruction & 0xff0) {
		case 0xe40:	Decode(Op::TRAP);		// 4-188 (p292)
		default:	break;
	}

	switch(instruction & 0xff8) {
		case 0x860:	Decode(Op::SWAP);			// 4-185 (p289)
		case 0x880:	Decode(Op::EXTbtow);		// 4-106 (p210)
		case 0x8c0:	Decode(Op::EXTwtol);		// 4-106 (p210)
		case 0xe50:	Decode(Op::LINKw);			// 4-111 (p215)
		case 0xe58:	Decode(Op::UNLINK);			// 4-194 (p298)
		case 0xe60:	Decode(Op::MOVEtoUSP);		// 6-21 (p475)
		case 0xe68:	Decode(Op::MOVEfromUSP);	// 6-21 (p475)
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

	switch(instruction & 0x0c0) {
		// 4-173 (p276), though this'll also hit DBcc 4-91 (p195)
		case 0x0c0:	Decode(Op::Scc);

		default:	break;
	}
	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode6(uint16_t instruction) {
	using Op = Operation;

	// 4-25 (p129), 4-59 (p163) and 4-55 (p159)
	Decode(Op::Bcc);
}

template <Model model>
Preinstruction Predecoder<model>::decode7(uint16_t instruction) {
	using Op = Operation;

	// 4-134 (p238)
	Decode(Op::MOVEq);
}

template <Model model>
Preinstruction Predecoder<model>::decode8(uint16_t instruction) {
	using Op = Operation;

	// 4-171 (p275)
	if((instruction & 0x1f0) == 0x100) Decode(Op::SBCD);

	// 4-150 (p254)
	switch(instruction & 0x0c0) {
		case 0x00:	Decode(Op::ORb);
		case 0x40:	Decode(Op::ORw);
		case 0x80:	Decode(Op::ORl);
		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	Decode(Op::DIVU);	// 4-97 (p201)
		case 0x1c0:	Decode(Op::DIVS);	// 4-93 (p197)
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decode9(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x0c0) {
		// 4-174 (p278)
		case 0x00:	Decode(Op::SUBb);
		case 0x40:	Decode(Op::SUBw);
		case 0x80:	Decode(Op::SUBl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-177 (p281)
		case 0x0c0:	Decode(Op::SUBAw);
		case 0x1c0:	Decode(Op::SUBAl);

		default:	break;
	}

	switch(instruction & 0x1f0) {
		// 4-184 (p288)
		case 0x100:	Decode(Op::SUBXb);
		case 0x140:	Decode(Op::SUBXw);
		case 0x180:	Decode(Op::SUBXl);

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

	switch(instruction & 0x0c0) {
		// 4-100 (p204)
		case 0x000:	Decode(Op::EORb);
		case 0x040:	Decode(Op::EORw);
		case 0x080:	Decode(Op::EORl);
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

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeC(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x1f0) {
		case 0x100:	Decode(Op::ABCD);	// 4-3 (p107)
		default:	break;
	}

	switch(instruction & 0x0c0) {
		// 4-15 (p119)
		case 0x00:	Decode(Op::ANDb);
		case 0x40:	Decode(Op::ANDw);
		case 0x80:	Decode(Op::ANDl);
		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	Decode(Op::MULU);	// 4-139 (p243)
		case 0x1c0:	Decode(Op::MULS);	// 4-136 (p240)
		default:	break;
	}

	// 4-105 (p209)
	switch(instruction & 0x1f8) {
		case 0x140:
		case 0x148:
		case 0x188:	Decode(Op::EXG);
		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeD(uint16_t instruction) {
	using Op = Operation;

	switch(instruction & 0x0c0) {
		// 4-4 (p108)
		case 0x000:	Decode(Op::ADDb);
		case 0x040:	Decode(Op::ADDw);
		case 0x080:	Decode(Op::ADDl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		// 4-7 (p111)
		case 0x0c0:	Decode(Op::ADDAw);
		case 0x1c0:	Decode(Op::ADDAl);

		default:	break;
	}

	switch(instruction & 0x1f0) {
		// 4-14 (p118)
		case 0x100:	Decode(Op::ADDXb);
		case 0x140:	Decode(Op::ADDXw);
		case 0x180:	Decode(Op::ADDXl);

		default:	break;
	}

	return Preinstruction();
}

template <Model model>
Preinstruction Predecoder<model>::decodeE(uint16_t instruction) {
	using Op = Operation;

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
