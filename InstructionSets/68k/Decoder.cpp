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
constexpr AddressingMode combined_mode(int mode, int reg) {
	return (mode != 7) ? AddressingMode(mode) : AddressingMode(0b01'000 | reg);
}

}

// MARK: - Instruction decoders.

/// Maps from an ExtendedOperation to an Operation; in practice that means that anything
/// that already is an Operation is passed through, and other things are mapped down into
/// an operation that doesn't duplicate detail about the operands that can be held by a
/// Preinstruction in other ways — for example, ANDI and AND are both represented by
/// a Preinstruction with an operation of AND, the former just happens to specify an
/// immediate operand.
constexpr Operation Predecoder::operation(uint8_t op) {
	if(op < uint8_t(Operation::Max)) {
		return Operation(op);
	}

	switch(op) {
		case MOVEMtoRl:	case MOVEMtoMl:	return Operation::MOVEMl;
		case MOVEMtoRw:	case MOVEMtoMw:	return Operation::MOVEMw;
		case MOVEPtoRl:	case MOVEPtoMl:	return Operation::MOVEPl;
		case MOVEPtoRw:	case MOVEPtoMw:	return Operation::MOVEPw;

		default: break;
	}

	return Operation::Undefined;
}

template <uint8_t op> Preinstruction Predecoder::decode(uint16_t instruction) {
	// Fields used pervasively below.
	//
	// Underlying assumption: the compiler will discard whatever of these
	// isn't actually used.
	const auto ea_register = instruction & 7;
	const auto ea_mode = (instruction >> 3) & 7;
	const auto ea_combined_mode = combined_mode(ea_mode, ea_register);

	const auto opmode = (instruction >> 6) & 7;
	const auto data_register = (instruction >> 9) & 7;
	constexpr auto operation = Predecoder::operation(op);

	switch(op) {

		//
		// MARK: ABCD, SBCD.
		//
		// 4-3 (p107), 4-171 (p275)
		case uint8_t(Operation::ABCD):	case uint8_t(Operation::SBCD):	{
			const auto addressing_mode = (instruction & 8) ?
				AddressingMode::AddressRegisterIndirectWithPredecrement : AddressingMode::DataRegisterDirect;

			return Preinstruction(operation,
				addressing_mode, ea_register,
				addressing_mode, data_register);
		}

		//
		// MARK: AND, OR, EOR.
		//
		case uint8_t(Operation::ANDb):	case uint8_t(Operation::ANDw):	case uint8_t(Operation::ANDl):
		case uint8_t(Operation::ORb):	case uint8_t(Operation::ORw):	case uint8_t(Operation::ORl):
		case uint8_t(Operation::EORb):	case uint8_t(Operation::EORw):	case uint8_t(Operation::EORl):	{
			// Opmode 7 is illegal.
			if(opmode == 7) {
				return Preinstruction();
			}

			constexpr bool is_eor =
				operation == Operation::EORb ||
				operation == Operation::EORw ||
				operation == Operation::EORl;

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
		// MARK: EXG.
		//
		case uint8_t(Operation::EXG):
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
		// MARK: MOVEfromSR, NBCD.
		//

		//
		// MARK: MULU, MULS, DIVU, DIVS.
		//
		case uint8_t(Operation::DIVU):	case uint8_t(Operation::DIVS):
		case uint8_t(Operation::MULU):	case uint8_t(Operation::MULS):
			return Preinstruction(operation,
				ea_combined_mode, ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		//
		// MARK: ORItoCCR, ORItoSR, ANDItoCCR, ANDItoSR, EORItoCCR, EORItoSR
		//
		case uint8_t(Operation::ORItoSR):	case uint8_t(Operation::ORItoCCR):
		case uint8_t(Operation::ANDItoSR):	case uint8_t(Operation::ANDItoCCR):
		case uint8_t(Operation::EORItoSR):	case uint8_t(Operation::EORItoCCR):
			return Preinstruction(operation,
				AddressingMode::ImmediateData, 0,
				operation == Operation::ORItoSR || operation == Operation::ANDItoSR || operation == Operation::EORItoSR);

		//
		// MARK: MOVEPtoRw, MOVEPtoRl
		//
		case MOVEPtoRw:	case MOVEPtoRl:
			return Preinstruction(operation,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register,
				AddressingMode::DataRegisterDirect, data_register);

		case MOVEPtoMw:	case MOVEPtoMl:
			return Preinstruction(operation,
				AddressingMode::DataRegisterDirect, data_register,
				AddressingMode::AddressRegisterIndirectWithDisplacement, ea_register);

		//
		// MARK: Impossible error case.
		//
		default:
			// Should be unreachable.
			assert(false);
	}

	// TODO: be careful that decoders for ADD, SUB, etc, must check the instruction a little
	// further to determine whether they're ADDI, SUBI, etc or the regular versions.

	// TODO: be willing to mutate Scc into DBcc.
}

// MARK: - Page decoders.

#define DecodeOp(y)		return decode<uint8_t(Operation::y)>(instruction)
#define DecodeEop(y)	return decode<y>(instruction)

Preinstruction Predecoder::decode0(uint16_t instruction) {
	switch(instruction & 0xfff) {
		case 0x03c:	DecodeOp(ORItoCCR);		// 4-155 (p259)
		case 0x07c:	DecodeOp(ORItoSR);		// 6-27 (p646)
		case 0x23c:	DecodeOp(ANDItoCCR);	// 4-20 (p124)
		case 0x27c:	DecodeOp(ANDItoSR);		// 6-2 (p456)
		case 0xa3c:	DecodeOp(EORItoCCR);	// 4-104 (p208)
		case 0xa7c:	DecodeOp(EORItoSR);		// 6-10 (p464)

		default:	break;
	}

	// TODO: determine whether it's useful to be able to flag these as immediate
	// versions here, rather than having it determined dynamically in decode.
	switch(instruction & 0xfc0) {
		// 4-153 (p257)
		case 0x000:	DecodeOp(ORb);
		case 0x040:	DecodeOp(ORw);
		case 0x080:	DecodeOp(ORl);

		// 4-18 (p122)
		case 0x200:	DecodeOp(ANDb);
		case 0x240:	DecodeOp(ANDw);
		case 0x280:	DecodeOp(ANDl);

		// 4-179 (p283)
		case 0x400:	DecodeOp(SUBb);
		case 0x440:	DecodeOp(SUBw);
		case 0x480:	DecodeOp(SUBl);

		// 4-9 (p113)
		case 0x600:	DecodeOp(ADDb);
		case 0x640:	DecodeOp(ADDw);
		case 0x680:	DecodeOp(ADDl);

		// 4-63 (p167)
		case 0x800:	DecodeOp(BTSTb);

		// 4-29 (p133)
		case 0x840:	DecodeOp(BCHGb);

		// 4-32 (p136)
		case 0x880:	DecodeOp(BCLRb);

		// 4-58 (p162)
		case 0x8c0:	DecodeOp(BSETb);

		// 4-102 (p206)
		case 0xa00:	DecodeOp(EORb);
		case 0xa40:	DecodeOp(EORw);
		case 0xa80:	DecodeOp(EORl);

		// 4-79 (p183)
		case 0xc00:	DecodeOp(CMPb);
		case 0xc40:	DecodeOp(CMPw);
		case 0xc80:	DecodeOp(CMPl);

		default:	break;
	}

	switch(instruction & 0x1c0) {
		case 0x100:	DecodeOp(BTSTb);	// 4-62 (p166)
		case 0x180:	DecodeOp(BCLRb);	// 4-31 (p135)

		case 0x140:	DecodeOp(BCHGb);	// 4-28 (p132)
		case 0x1c0:	DecodeOp(BSETb);	// 4-57 (p161)

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

Preinstruction Predecoder::decode1(uint16_t instruction) {
	DecodeOp(MOVEb);
}

Preinstruction Predecoder::decode2(uint16_t instruction) {
	DecodeOp(MOVEl);
}

Preinstruction Predecoder::decode3(uint16_t instruction) {
	DecodeOp(MOVEw);
}

Preinstruction Predecoder::decode4(uint16_t instruction) {
	switch(instruction & 0xfff) {
		case 0xe70:	DecodeOp(RESET);	// 6-83 (p537)
		case 0xe71:	DecodeOp(NOP);		// 8-13 (p469)
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

		// 4-144 (p248)
		case 0x400:	DecodeOp(NEGb);
		case 0x440:	DecodeOp(NEGw);
		case 0x480:	DecodeOp(NEGl);

		// 4-123 (p227)
		case 0x4c0:	DecodeOp(MOVEtoCCR);

		// 4-148 (p250)
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
		case 0x1c0:	DecodeOp(MOVEAl);	// 4-110 (p214)
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
		case 0xe50:	DecodeOp(LINK);			// 4-111 (p215)
		case 0xe58:	DecodeOp(UNLINK);		// 4-194 (p298)
		case 0xe60:	DecodeOp(MOVEtoUSP);	// 6-21 (p475)
		case 0xe68:	DecodeOp(MOVEfromUSP);	// 6-21 (p475)
		default:	break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decode5(uint16_t instruction) {
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

Preinstruction Predecoder::decode6(uint16_t instruction) {
	// 4-25 (p129), 4-59 (p163) and 4-55 (p159)
	DecodeOp(Bcc);
}

Preinstruction Predecoder::decode7(uint16_t instruction) {
	// 4-134 (p238)
	DecodeEop(MOVEq);
}

Preinstruction Predecoder::decode8(uint16_t instruction) {
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

Preinstruction Predecoder::decode9(uint16_t instruction) {
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

Preinstruction Predecoder::decodeA(uint16_t) {
	return Preinstruction();
}

Preinstruction Predecoder::decodeB(uint16_t instruction) {
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

Preinstruction Predecoder::decodeC(uint16_t instruction) {
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

Preinstruction Predecoder::decodeD(uint16_t instruction) {
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

Preinstruction Predecoder::decodeE(uint16_t instruction) {
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

Preinstruction Predecoder::decodeF(uint16_t) {
	return Preinstruction();
}

#undef DecodeOp

// MARK: - Main decoder.

Preinstruction Predecoder::decode(uint16_t instruction) {
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
