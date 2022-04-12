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
}

// MARK: - Page decoders.

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
//		case 0xe60: return decode<Operation::MOVEtoUSP>(instruction);	// 6-21 (p475)
		default: break;
	}

	// TODO: determine MOVEtoUSP and MOVEfromUSP.

	switch(instruction & 0xff8) {
		case 0xe60: return decode<Operation::SWAP>(instruction);		// 4-185 (p289)
		case 0x880: return decode<Operation::EXTbtow>(instruction);		// 4-106 (p210)
		case 0x8c0: return decode<Operation::EXTwtol>(instruction);		// 4-106 (p210)
		case 0xe50: return decode<Operation::LINK>(instruction);		// 4-111 (p215)
		case 0xe58: return decode<Operation::UNLINK>(instruction);		// 4-194 (p298)
		default: break;
	}

	return Preinstruction();
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

Preinstruction Predecoder::decodeB(uint16_t instruction) {
	// 4-100 (p204)
	switch(instruction & 0x0c0) {
		case 0x000:	return decode<Operation::EORb>(instruction);
		case 0x040:	return decode<Operation::EORw>(instruction);
		case 0x080:	return decode<Operation::EORl>(instruction);
		default: break;
	}

	// 4-75 (p179)
	switch(instruction & 0x1c0) {
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
	// 4-3 (p107)
	if((instruction & 0x1f0) == 0x100) return decode<Operation::ABCD>(instruction);

	// 4-15 (p119)
	switch(instruction & 0x0c0) {
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
	}

	return Preinstruction();
}

// MARK: - Main decoder.

Preinstruction Predecoder::decode(uint16_t instruction) {
	// Divide first based on line.
	switch(instruction & 0xf000) {
		case 0x4000:	return decode4(instruction);
		case 0x8000:	return decode8(instruction);
		case 0xb000:	return decodeB(instruction);
		case 0xc000:	return decodeC(instruction);

		default: break;
	}

	return Preinstruction();
}
