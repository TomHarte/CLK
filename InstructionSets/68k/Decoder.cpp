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

template <Operation operation> Preinstruction Predecoder::decode(uint16_t instruction) {
	// Fields used pervasively below.
	//
	// Underlying assumption: the compiler will discard whatever of these
	// isn't actually used.
	const auto ea_register = instruction & 7;
	const auto ea_mode = (instruction >> 3) & 7;
	const auto ea_combined_mode = combined_mode(ea_mode, ea_register);

	const auto opmode = (instruction >> 6) & 7;
	const auto data_register = (instruction >> 9) & 7;

	switch(operation) {

		//
		// MARK: ABCD, SBCD.
		//
		case Operation::ABCD:	case Operation::SBCD: {
			const auto addressing_mode = (instruction & 8) ?
				AddressingMode::AddressRegisterIndirectWithPredecrement : AddressingMode::DataRegisterDirect;

			return Preinstruction(operation,
				addressing_mode, ea_register,
				addressing_mode, data_register);
		}

		//
		// MARK: AND, OR, EOR.
		//
		case Operation::ANDb:	case Operation::ANDw:	case Operation::ANDl:
		case Operation::ORb:	case Operation::ORw:	case Operation::ORl:
		case Operation::EORb:	case Operation::EORw:	case Operation::EORl: {
			// Opmode 7 is illegal.
			if(opmode == 7) {
				return Preinstruction();
			}

			constexpr bool is_eor = operation == Operation::EORb || operation == Operation::EORw || operation == Operation::EORl;

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
		case Operation::EXG:
			switch((instruction >> 3)&31) {
				default: 	return Preinstruction();

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
		case Operation::DIVU:	case Operation::DIVS:
		case Operation::MULU:	case Operation::MULS:
			return Preinstruction(operation,
				ea_combined_mode, ea_register,
				AddressingMode::DataRegisterDirect, data_register);


		default:
			// Should be unreachable.
			assert(false);
	}

	// TODO: be careful that decoders for ADD, SUB, etc, must check the instruction a little
	// further to determine whether they're ADDI, SUBI, etc or the regular versions.

	// TODO: be willing to mutate Scc into DBcc.
}

// MARK: - Page decoders.

Preinstruction Predecoder::decode0(uint16_t instruction) {
	switch(instruction) {
		case 0x003c: return decode<Operation::ORItoCCR>(instruction);
		case 0x007c: return decode<Operation::ORItoSR>(instruction);
		case 0x023c: return decode<Operation::ANDItoCCR>(instruction);
		case 0x027c: return decode<Operation::ANDItoSR>(instruction);
		case 0x0a3c: return decode<Operation::EORItoCCR>(instruction);
		case 0x0a7c: return decode<Operation::EORItoSR>(instruction);

		default: break;
	}

	switch(instruction & 0xfc0) {
		// 4-153 (p257)
		case 0x000: return decode<Operation::ORb>(instruction);
		case 0x040: return decode<Operation::ORw>(instruction);
		case 0x080: return decode<Operation::ORl>(instruction);

		// 4-18 (p122)
		case 0x200: return decode<Operation::ANDb>(instruction);
		case 0x240: return decode<Operation::ANDw>(instruction);
		case 0x280: return decode<Operation::ANDl>(instruction);

		// 4-179 (p283)
		case 0x400: return decode<Operation::SUBb>(instruction);
		case 0x440: return decode<Operation::SUBw>(instruction);
		case 0x480: return decode<Operation::SUBl>(instruction);

		// 4-9 (p113)
		case 0x600: return decode<Operation::ADDb>(instruction);
		case 0x640: return decode<Operation::ADDw>(instruction);
		case 0x680: return decode<Operation::ADDl>(instruction);

		// 4-63 (p167)
		case 0x800: return decode<Operation::BTSTb>(instruction);

		// 4-29 (p133)
		case 0x840: return decode<Operation::BCHGb>(instruction);

		// 4-32 (p136)
		case 0x880: return decode<Operation::BCLRb>(instruction);

		// 4-58 (p162)
		case 0x8c0: return decode<Operation::BSETb>(instruction);

		// 4-102 (p206)
		case 0xa00: return decode<Operation::EORb>(instruction);
		case 0xa40: return decode<Operation::EORw>(instruction);
		case 0xa80: return decode<Operation::EORl>(instruction);

		// 4-79 (p183)
		case 0xc00: return decode<Operation::CMPb>(instruction);
		case 0xc40: return decode<Operation::CMPw>(instruction);
		case 0xc80: return decode<Operation::CMPl>(instruction);

		default: break;
	}

	switch(instruction & 0x1c0) {
		case 0x100: return decode<Operation::BTSTb>(instruction);	// 4-62 (p166)
		case 0x180: return decode<Operation::BCLRb>(instruction);	// 4-31 (p135)

		case 0x140: return decode<Operation::BCHGb>(instruction);	// 4-28 (p132)
		case 0x1c0: return decode<Operation::BSETb>(instruction);	// 4-57 (p161)

		default: break;
	}

	switch(instruction & 0x1f8) {
		// 4-133 (p237)
		case 0x108:	return decode<Operation::MOVEPtoRw>(instruction);
		case 0x148:	return decode<Operation::MOVEPtoRl>(instruction);
		case 0x188:	return decode<Operation::MOVEPtoMw>(instruction);
		case 0x1c8:	return decode<Operation::MOVEPtoMl>(instruction);

		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decode1(uint16_t instruction) {
	return decode<Operation::MOVEb>(instruction);
}

Preinstruction Predecoder::decode2(uint16_t instruction) {
	return decode<Operation::MOVEl>(instruction);
}

Preinstruction Predecoder::decode3(uint16_t instruction) {
	return decode<Operation::MOVEw>(instruction);
}

Preinstruction Predecoder::decode4(uint16_t instruction) {
	switch(instruction & 0xfff) {
		case 0xe70:	return decode<Operation::RESET>(instruction);	// 6-83 (p537)
		case 0xe71:	return decode<Operation::NOP>(instruction);		// 8-13 (p469)
		case 0xe73:	return decode<Operation::RTE>(instruction);		// 6-84 (p538)
		case 0xe75:	return decode<Operation::RTS>(instruction);		// 4-169 (p273)
		case 0xe76:	return decode<Operation::TRAPV>(instruction);	// 4-191 (p295)
		case 0xe77:	return decode<Operation::RTR>(instruction);		// 4-168 (p272)
		default: break;
	}

	switch(instruction & 0xfc0) {
		// 4-146 (p250)
		case 0x000: return decode<Operation::NEGXb>(instruction);
		case 0x040: return decode<Operation::NEGXw>(instruction);
		case 0x080: return decode<Operation::NEGXl>(instruction);

		// 6-17 (p471)
		case 0x0c0: return decode<Operation::MOVEfromSR>(instruction);

		// 4-73 (p177)
		case 0x200: return decode<Operation::CLRb>(instruction);
		case 0x240: return decode<Operation::CLRw>(instruction);
		case 0x280: return decode<Operation::CLRl>(instruction);

		// 4-144 (p248)
		case 0x400: return decode<Operation::NEGb>(instruction);
		case 0x440: return decode<Operation::NEGw>(instruction);
		case 0x480: return decode<Operation::NEGl>(instruction);

		// 4-123 (p227)
		case 0x4c0: return decode<Operation::MOVEtoCCR>(instruction);

		// 4-148 (p250)
		case 0x600: return decode<Operation::NOTb>(instruction);
		case 0x640: return decode<Operation::NOTw>(instruction);
		case 0x680: return decode<Operation::NOTl>(instruction);

		// 4-123 (p227)
		case 0x6c0: return decode<Operation::MOVEtoSR>(instruction);

		// 4-142 (p246)
		case 0x800: return decode<Operation::NBCD>(instruction);

		// 4-159 (p263)
		case 0x840: return decode<Operation::PEA>(instruction);

		// 4-128 (p232)
		case 0x880:	return decode<Operation::MOVEMtoMw>(instruction);
		case 0x8c0:	return decode<Operation::MOVEMtoMl>(instruction);
		case 0xc80:	return decode<Operation::MOVEMtoRw>(instruction);
		case 0xcc0:	return decode<Operation::MOVEMtoRl>(instruction);

		// 4-192 (p296)
		case 0xa00: return decode<Operation::TSTb>(instruction);
		case 0xa40: return decode<Operation::TSTw>(instruction);
		case 0xa80: return decode<Operation::TSTl>(instruction);

		// 4-186 (p290)
		case 0xac0: return decode<Operation::TAS>(instruction);

		// 4-109 (p213)
		case 0xe80: return decode<Operation::JSR>(instruction);

		// 4-108 (p212)
		case 0xec0: return decode<Operation::JMP>(instruction);

		default: break;
	}

	switch(instruction & 0x1c0) {
		case 0x1c0: return decode<Operation::MOVEAl>(instruction);	// 4-110 (p214)
		case 0x180: return decode<Operation::CHK>(instruction);		// 4-69 (p173)
		default: break;
	}

	switch(instruction & 0xff0) {
		case 0xe40: return decode<Operation::TRAP>(instruction);		// 4-188 (p292)
		default: break;
	}

	switch(instruction & 0xff8) {
		case 0x860: return decode<Operation::SWAP>(instruction);		// 4-185 (p289)
		case 0x880: return decode<Operation::EXTbtow>(instruction);		// 4-106 (p210)
		case 0x8c0: return decode<Operation::EXTwtol>(instruction);		// 4-106 (p210)
		case 0xe50: return decode<Operation::LINK>(instruction);		// 4-111 (p215)
		case 0xe58: return decode<Operation::UNLINK>(instruction);		// 4-194 (p298)
		case 0xe60: return decode<Operation::MOVEtoUSP>(instruction);	// 6-21 (p475)
		case 0xe68: return decode<Operation::MOVEfromUSP>(instruction);	// 6-21 (p475)
		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decode5(uint16_t instruction) {
	switch(instruction & 0x1c0) {
		// 4-11 (p115)
		case 0x000: return decode<Operation::ADDQb>(instruction);
		case 0x040: return decode<Operation::ADDQw>(instruction);
		case 0x080: return decode<Operation::ADDQl>(instruction);

		// 4-181 (p285)
		case 0x100: return decode<Operation::SUBQb>(instruction);
		case 0x140: return decode<Operation::SUBQw>(instruction);
		case 0x180: return decode<Operation::SUBQl>(instruction);

		default: break;
	}

	switch(instruction & 0x0c0) {
		// 4-173 (p276), though this'll also hit DBcc 4-91 (p195)
		case 0x0c0: return decode<Operation::Scc>(instruction);

		default: break;
	}
	return Preinstruction();
}

Preinstruction Predecoder::decode6(uint16_t instruction) {
	// 4-25 (p129), 4-59 (p163) and 4-55 (p159)
	return decode<Operation::Bcc>(instruction);
}

Preinstruction Predecoder::decode7(uint16_t instruction) {
	// 4-134 (p238)
	return decode<Operation::MOVEq>(instruction);
}

Preinstruction Predecoder::decode8(uint16_t instruction) {
	// 4-171 (p275)
	if((instruction & 0x1f0) == 0x100) return decode<Operation::SBCD>(instruction);

	// 4-150 (p254)
	switch(instruction & 0x0c0) {
		case 0x00:	return decode<Operation::ORb>(instruction);
		case 0x40:	return decode<Operation::ORw>(instruction);
		case 0x80:	return decode<Operation::ORl>(instruction);
		default: break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	return decode<Operation::DIVU>(instruction);	// 4-97 (p201)
		case 0x1c0:	return decode<Operation::DIVS>(instruction);	// 4-93 (p197)
		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decode9(uint16_t instruction) {
	switch(instruction & 0x0c0) {
		// 4-174 (p278)
		case 0x00:	return decode<Operation::SUBb>(instruction);
		case 0x40:	return decode<Operation::SUBw>(instruction);
		case 0x80:	return decode<Operation::SUBl>(instruction);

		default: break;
	}

	switch(instruction & 0x1c0) {
		// 4-177 (p281)
		case 0x0c0:	return decode<Operation::SUBAw>(instruction);
		case 0x1c0:	return decode<Operation::SUBAl>(instruction);

		default: break;
	}

	switch(instruction & 0x1f0) {
		// 4-184 (p288)
		case 0x100:	return decode<Operation::SUBXb>(instruction);
		case 0x140:	return decode<Operation::SUBXw>(instruction);
		case 0x180:	return decode<Operation::SUBXl>(instruction);

		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decodeA(uint16_t) {
	return Preinstruction();
}

Preinstruction Predecoder::decodeB(uint16_t instruction) {
	switch(instruction & 0x0c0) {
		// 4-100 (p204)
		case 0x000:	return decode<Operation::EORb>(instruction);
		case 0x040:	return decode<Operation::EORw>(instruction);
		case 0x080:	return decode<Operation::EORl>(instruction);
		default: break;
	}

	switch(instruction & 0x1c0) {
		// 4-75 (p179)
		case 0x000:	return decode<Operation::CMPb>(instruction);
		case 0x040:	return decode<Operation::CMPw>(instruction);
		case 0x080:	return decode<Operation::CMPl>(instruction);

		// 4-77 (p181)
		case 0x0c0:	return decode<Operation::CMPAw>(instruction);
		case 0x1c0:	return decode<Operation::CMPAl>(instruction);

		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decodeC(uint16_t instruction) {
	switch(instruction & 0x1f0) {
		case 0x100: return decode<Operation::ABCD>(instruction);	// 4-3 (p107)
		default: break;
	}

	switch(instruction & 0x0c0) {
		// 4-15 (p119)
		case 0x00:	return decode<Operation::ANDb>(instruction);
		case 0x40:	return decode<Operation::ANDw>(instruction);
		case 0x80:	return decode<Operation::ANDl>(instruction);
		default: break;
	}

	switch(instruction & 0x1c0) {
		case 0x0c0:	return decode<Operation::MULU>(instruction);	// 4-139 (p243)
		case 0x1c0:	return decode<Operation::MULS>(instruction);	// 4-136 (p240)
		default: break;
	}

	// 4-105 (p209)
	switch(instruction & 0x1f8) {
		case 0x140:
		case 0x148:
		case 0x188:	return decode<Operation::EXG>(instruction);
		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decodeD(uint16_t instruction) {
	switch(instruction & 0x0c0) {
		// 4-4 (p108)
		case 0x000:	return decode<Operation::ADDb>(instruction);
		case 0x040:	return decode<Operation::ADDw>(instruction);
		case 0x080:	return decode<Operation::ADDl>(instruction);

		default: break;
	}

	switch(instruction & 0x1c0) {
		// 4-7 (p111)
		case 0x0c0:	return decode<Operation::ADDAw>(instruction);
		case 0x1c0:	return decode<Operation::ADDAl>(instruction);

		default: break;
	}

	switch(instruction & 0x1f0) {
		// 4-14 (p118)
		case 0x100:	return decode<Operation::ADDXb>(instruction);
		case 0x140:	return decode<Operation::ADDXw>(instruction);
		case 0x180:	return decode<Operation::ADDXl>(instruction);

		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decodeE(uint16_t instruction) {
	switch(instruction & 0x1d8) {
		// 4-22 (p126)
		case 0x000:	return decode<Operation::ASRb>(instruction);
		case 0x040:	return decode<Operation::ASRw>(instruction);
		case 0x080:	return decode<Operation::ASRl>(instruction);

		// 4-113 (p217)
		case 0x008:	return decode<Operation::LSRb>(instruction);
		case 0x048:	return decode<Operation::LSRw>(instruction);
		case 0x088:	return decode<Operation::LSRl>(instruction);

		// 4-163 (p267)
		case 0x010:	return decode<Operation::ROXRb>(instruction);
		case 0x050:	return decode<Operation::ROXRw>(instruction);
		case 0x090:	return decode<Operation::ROXRl>(instruction);

		// 4-160 (p264)
		case 0x018:	return decode<Operation::RORb>(instruction);
		case 0x058:	return decode<Operation::RORw>(instruction);
		case 0x098:	return decode<Operation::RORl>(instruction);

		// 4-22 (p126)
		case 0x100:	return decode<Operation::ASLb>(instruction);
		case 0x140:	return decode<Operation::ASLw>(instruction);
		case 0x180:	return decode<Operation::ASLl>(instruction);

		// 4-113 (p217)
		case 0x108:	return decode<Operation::LSLb>(instruction);
		case 0x148:	return decode<Operation::LSLw>(instruction);
		case 0x188:	return decode<Operation::LSLl>(instruction);

		// 4-163 (p267)
		case 0x110:	return decode<Operation::ROXLb>(instruction);
		case 0x150:	return decode<Operation::ROXLw>(instruction);
		case 0x190:	return decode<Operation::ROXLl>(instruction);

		// 4-160 (p264)
		case 0x118:	return decode<Operation::ROLb>(instruction);
		case 0x158:	return decode<Operation::ROLw>(instruction);
		case 0x198:	return decode<Operation::ROLl>(instruction);

		default: break;
	}

	switch(instruction & 0xfc0) {
		case 0x0c0:	return decode<Operation::ASRm>(instruction);	// 4-22 (p126)
		case 0x1c0:	return decode<Operation::ASLm>(instruction);	// 4-22 (p126)
		case 0x2c0:	return decode<Operation::LSRm>(instruction);	// 4-113 (p217)
		case 0x3c0:	return decode<Operation::LSLm>(instruction);	// 4-113 (p217)
		case 0x4c0:	return decode<Operation::ROXRm>(instruction);	// 4-163 (p267)
		case 0x5c0:	return decode<Operation::ROXLm>(instruction);	// 4-163 (p267)
		case 0x6c0:	return decode<Operation::RORm>(instruction);	// 4-160 (p264)
		case 0x7c0:	return decode<Operation::ROLm>(instruction);	// 4-160 (p264)

		default: break;
	}

	return Preinstruction();
}

Preinstruction Predecoder::decodeF(uint16_t) {
	return Preinstruction();
}

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

		default: break;
	}

	return Preinstruction();
}
