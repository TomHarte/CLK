//
//  Instruction.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "Instruction.hpp"

using namespace InstructionSet::x86;

std::string InstructionSet::x86::to_string(Source source, DataSize size) {
	switch(source) {
		case Source::eAX: {
			constexpr char sizes[][4] = { "al", "ax", "eax", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eCX: {
			constexpr char sizes[][4] = { "cl", "cx", "ecx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDX: {
			constexpr char sizes[][4] = { "dl", "dx", "edx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBX: {
			constexpr char sizes[][4] = { "bl", "bx", "ebx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSPorAH: {
			constexpr char sizes[][4] = { "ah", "sp", "esp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBPorCH: {
			constexpr char sizes[][4] = { "ch", "bp", "ebp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSIorDH: {
			constexpr char sizes[][4] = { "dh", "si", "esi", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDIorBH: {
			constexpr char sizes[][4] = { "bh", "di", "edi", "?" };
			return sizes[static_cast<int>(size)];
		}

		case Source::ES:	return "es";
		case Source::CS:	return "cs";
		case Source::SS:	return "ss";
		case Source::DS:	return "ds";
		case Source::FS:	return "fd";
		case Source::GS:	return "gs";

		case Source::None:				return "0";
		case Source::DirectAddress:		return "DirectAccess";
		case Source::Immediate:			return "Immediate";
		case Source::Indirect:			return "Indirect";
		case Source::IndirectNoBase:	return "IndirectNoBase";

		default: return "???";
	}
}

