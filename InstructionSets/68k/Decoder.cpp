//
//  Decoder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

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

	}
}

// MARK: - Page decoders.

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
		case 0x8000:	return decode8(instruction);
		case 0xc000:	return decodeC(instruction);

		default: break;
	}

	return Preinstruction();
}
