//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Model.hpp"

#include <type_traits>

namespace CPU::MOS6502Mk2 {

enum class Operation {
	BRK,
	NOP,	JAM,

	ORA,	AND,	EOR,
	INS,	ADC,	SBC,
	CMP,	CPX,	CPY,
	BIT,	BITNoNV,
	LDA,	LDX,	LDY,	LAX,
	STA,	STX,	STY,	STZ,	SAX,	SHA,	SHX,	SHY,	SHS,
	ASL,	ASO,	ROL,	RLA,	LSR,	LSE,	ASR,	ROR,	RRA,
	CLC,	CLI,	CLV,	CLD,	SEC,	SEI,	SED,
	RMB,	SMB,	TRB,	TSB,
	INC,	DEC,	INX,	DEX,	INY,	DEY,	INA,	DEA,	DCP,
	BPL,	BMI,	BVC,	BVS,	BCC,	BCS,	BNE,	BEQ,	BRA,
	BBRBBS,
	TXA,	TYA,	TXS,	TAY,	TAX,	TSX,
	ARR,	SBX,	LXA,	ANE,	ANC,	LAS,
	JSR,	RTI,	RTS,

	PHP,	PLP,	JMP,
};

enum class AccessProgram {
	Implied,
	Immediate,
	Accumulator,
	Relative,

	Push,
	Pull,

	AbsoluteRead,
	AbsoluteModify,
	AbsoluteWrite,
	AbsoluteJMP,
	AbsoluteIndirectJMP,

	AbsoluteXRead,
	AbsoluteXModify,
	AbsoluteXWrite,
	AbsoluteXNOP,

	AbsoluteYRead,
	AbsoluteYModify,
	AbsoluteYWrite,

	ZeroRead,
	ZeroModify,
	ZeroWrite,

	ZeroXRead,
	ZeroXModify,
	ZeroXWrite,

	ZeroYRead,
	ZeroYModify,
	ZeroYWrite,

	ZeroIndirectRead,
	ZeroIndirectWrite,

	IndexedIndirectRead,
	IndexedIndirectModify,
	IndexedIndirectWrite,

	IndirectIndexedRead,
	IndirectIndexedModify,
	IndirectIndexedWrite,

	// Irregular.
	BRK,	JSR,	RTI,	RTS,	JAM,

	Max,
};

struct Instruction {
	AccessProgram program;
	Operation operation;
};

template <Model model, typename Enable = void> struct Decoder;

template <Model model>
struct Decoder<model, std::enable_if_t<is_6502(model)>> {
	static constexpr Instruction decode(const uint8_t opcode) {
		using enum AccessProgram;
		switch(opcode) {
			case 0x00:	return {BRK, Operation::BRK};
			case 0x20:	return {JSR, Operation::JSR};
			case 0x40:	return {RTI, Operation::RTI};
			case 0x60:	return {RTS, Operation::RTS};
			case 0x80:	return {Immediate, Operation::NOP};
			case 0xa0:	return {Immediate, Operation::LDY};
			case 0xc0:	return {Immediate, Operation::CPY};
			case 0xe0:	return {Immediate, Operation::CPX};

			case 0x01:	return {IndexedIndirectRead, Operation::ORA};
			case 0x21:	return {IndexedIndirectRead, Operation::AND};
			case 0x41:	return {IndexedIndirectRead, Operation::EOR};
			case 0x61:	return {IndexedIndirectRead, Operation::ADC};
			case 0x81:	return {IndexedIndirectWrite, Operation::STA};
			case 0xa1:	return {IndexedIndirectRead, Operation::LDA};
			case 0xc1:	return {IndexedIndirectRead, Operation::CMP};
			case 0xe1:	return {IndexedIndirectRead, Operation::SBC};

			case 0x02:	return {JAM, Operation::JAM};
			case 0x22:	return {JAM, Operation::JAM};
			case 0x42:	return {JAM, Operation::JAM};
			case 0x62:	return {JAM, Operation::JAM};
			case 0x82:	return {Immediate, Operation::NOP};
			case 0xa2:	return {Immediate, Operation::LDX};
			case 0xc2:	return {Implied, Operation::NOP};
			case 0xe2:	return {Implied, Operation::NOP};

			case 0x03:	return {IndexedIndirectModify, Operation::ASO};
			case 0x23:	return {IndexedIndirectModify, Operation::RLA};
			case 0x43:	return {IndexedIndirectModify, Operation::LSE};
			case 0x63:	return {IndexedIndirectModify, Operation::RRA};
			case 0x83:	return {IndexedIndirectWrite, Operation::SAX};
			case 0xa3:	return {IndexedIndirectRead, Operation::LAX};
			case 0xc3:	return {IndexedIndirectWrite, Operation::DCP};
			case 0xe3:	return {IndexedIndirectWrite, Operation::INS};

			case 0x04:	return {ZeroRead, Operation::NOP};
			case 0x24:	return {ZeroRead, Operation::BIT};
			case 0x44:	return {ZeroRead, Operation::NOP};
			case 0x64:	return {ZeroRead, Operation::NOP};
			case 0x84:	return {ZeroWrite, Operation::STY};
			case 0xa4:	return {ZeroRead, Operation::LDY};
			case 0xc4:	return {ZeroRead, Operation::CPY};
			case 0xe4:	return {ZeroRead, Operation::CPX};

			case 0x05:	return {ZeroRead, Operation::ORA};
			case 0x25:	return {ZeroRead, Operation::AND};
			case 0x45:	return {ZeroRead, Operation::EOR};
			case 0x65:	return {ZeroRead, Operation::ADC};
			case 0x85:	return {ZeroWrite, Operation::STA};
			case 0xa5:	return {ZeroRead, Operation::LDA};
			case 0xc5:	return {ZeroRead, Operation::CMP};
			case 0xe5:	return {ZeroRead, Operation::SBC};

			case 0x06:	return {ZeroModify, Operation::ASL};
			case 0x26:	return {ZeroModify, Operation::ROL};
			case 0x46:	return {ZeroModify, Operation::LSR};
			case 0x66:	return {ZeroModify, Operation::ROR};
			case 0x86:	return {ZeroWrite, Operation::STX};
			case 0xa6:	return {ZeroRead, Operation::LDX};
			case 0xc6:	return {ZeroModify, Operation::DEC};
			case 0xe6:	return {ZeroModify, Operation::INC};

			case 0x07:	return {ZeroModify, Operation::ASO};
			case 0x27:	return {ZeroModify, Operation::RLA};
			case 0x47:	return {ZeroModify, Operation::LSE};
			case 0x67:	return {ZeroModify, Operation::RRA};
			case 0x87:	return {ZeroWrite, Operation::SAX};
			case 0xa7:	return {ZeroRead, Operation::LDX};
			case 0xc7:	return {ZeroRead, Operation::LAX};
			case 0xe7:	return {ZeroModify, Operation::INS};

			case 0x08:	return {Push, Operation::PHP};
			case 0x28:	return {Pull, Operation::PLP};
			case 0x48:	return {Push, Operation::STA};
			case 0x68:	return {Pull, Operation::LDA};
			case 0x88:	return {Implied, Operation::DEY};
			case 0xa8:	return {Implied, Operation::TAY};
			case 0xc8:	return {Implied, Operation::INY};
			case 0xe8:	return {Implied, Operation::INX};

			case 0x09:	return {Immediate, Operation::ORA};
			case 0x29:	return {Immediate, Operation::AND};
			case 0x49:	return {Immediate, Operation::EOR};
			case 0x69:	return {Immediate, Operation::ADC};
			case 0x89:	return {Immediate, Operation::NOP};
			case 0xa9:	return {Immediate, Operation::LDA};
			case 0xc9:	return {Immediate, Operation::CMP};
			case 0xe9:	return {Immediate, Operation::SBC};

			case 0x0a:	return {Accumulator, Operation::ASL};
			case 0x2a:	return {Accumulator, Operation::ROL};
			case 0x4a:	return {Accumulator, Operation::LSR};
			case 0x6a:	return {Accumulator, Operation::ROR};
			case 0x8a:	return {Implied, Operation::TXA};
			case 0xaa:	return {Implied, Operation::TAX};
			case 0xca:	return {Implied, Operation::DEX};
			case 0xea:	return {Implied, Operation::NOP};

			case 0x0b:	return {Immediate, Operation::ANC};
			case 0x2b:	return {Immediate, Operation::ANC};
			case 0x4b:	return {Immediate, Operation::ASR};
			case 0x6b:	return {Immediate, Operation::ARR};
			case 0x8b:	return {Immediate, Operation::ANE};
			case 0xab:	return {Immediate, Operation::LXA};
			case 0xcb:	return {Immediate, Operation::SBX};
			case 0xeb:	return {Immediate, Operation::SBC};

			case 0x0c:	return {AbsoluteRead, Operation::NOP};
			case 0x2c:	return {AbsoluteRead, Operation::BIT};
			case 0x4c:	return {AbsoluteJMP, Operation::JMP};
			case 0x6c:	return {AbsoluteIndirectJMP, Operation::JMP};
			case 0x8c:	return {AbsoluteWrite, Operation::STY};
			case 0xac:	return {AbsoluteRead, Operation::LDY};
			case 0xcc:	return {AbsoluteRead, Operation::CPY};
			case 0xec:	return {AbsoluteRead, Operation::CPX};

			case 0x0d:	return {AbsoluteRead, Operation::ORA};
			case 0x2d:	return {AbsoluteRead, Operation::AND};
			case 0x4d:	return {AbsoluteRead, Operation::EOR};
			case 0x6d:	return {AbsoluteRead, Operation::ADC};
			case 0x8d:	return {AbsoluteWrite, Operation::STA};
			case 0xad:	return {AbsoluteRead, Operation::LDA};
			case 0xcd:	return {AbsoluteRead, Operation::CMP};
			case 0xed:	return {AbsoluteRead, Operation::SBC};

			case 0x0e:	return {AbsoluteModify, Operation::ASL};
			case 0x2e:	return {AbsoluteModify, Operation::ROL};
			case 0x4e:	return {AbsoluteModify, Operation::LSR};
			case 0x6e:	return {AbsoluteModify, Operation::ROR};
			case 0x8e:	return {AbsoluteWrite, Operation::STX};
			case 0xae:	return {AbsoluteRead, Operation::LDX};
			case 0xce:	return {AbsoluteModify, Operation::DEC};
			case 0xee:	return {AbsoluteModify, Operation::INC};

			case 0x0f:	return {AbsoluteModify, Operation::ASO};
			case 0x2f:	return {AbsoluteModify, Operation::RLA};
			case 0x4f:	return {AbsoluteModify, Operation::LSE};
			case 0x6f:	return {AbsoluteModify, Operation::RRA};
			case 0x8f:	return {AbsoluteWrite, Operation::SAX};
			case 0xaf:	return {AbsoluteRead, Operation::LAX};
			case 0xcf:	return {AbsoluteModify, Operation::DCP};
			case 0xef:	return {AbsoluteModify, Operation::INS};

			case 0x10:	return {Relative, Operation::BPL};
			case 0x30:	return {Relative, Operation::BMI};
			case 0x50:	return {Relative, Operation::BVC};
			case 0x70:	return {Relative, Operation::BVS};
			case 0x90:	return {Relative, Operation::BCC};
			case 0xb0:	return {Relative, Operation::BCS};
			case 0xd0:	return {Relative, Operation::BNE};
			case 0xf0:	return {Relative, Operation::BEQ};

			case 0x11:	return {IndirectIndexedRead, Operation::ORA};
			case 0x31:	return {IndirectIndexedRead, Operation::AND};
			case 0x51:	return {IndirectIndexedRead, Operation::EOR};
			case 0x71:	return {IndirectIndexedRead, Operation::ADC};
			case 0x91:	return {IndirectIndexedRead, Operation::STA};
			case 0xb1:	return {IndirectIndexedRead, Operation::LDA};
			case 0xd1:	return {IndirectIndexedRead, Operation::CMP};
			case 0xf1:	return {IndirectIndexedRead, Operation::SBC};

			case 0x12:	return {JAM, Operation::JAM};
			case 0x32:	return {JAM, Operation::JAM};
			case 0x52:	return {JAM, Operation::JAM};
			case 0x72:	return {JAM, Operation::JAM};
			case 0x92:	return {JAM, Operation::JAM};
			case 0xb2:	return {JAM, Operation::JAM};
			case 0xd2:	return {JAM, Operation::JAM};
			case 0xf2:	return {JAM, Operation::JAM};

			case 0x13:	return {IndirectIndexedModify, Operation::ASO};
			case 0x33:	return {IndirectIndexedModify, Operation::RLA};
			case 0x53:	return {IndirectIndexedModify, Operation::LSE};
			case 0x73:	return {IndirectIndexedModify, Operation::RRA};
			case 0x93:	return {IndirectIndexedWrite, Operation::SHA};
			case 0xb3:	return {IndirectIndexedRead, Operation::LAX};
			case 0xd3:	return {IndirectIndexedModify, Operation::DCP};
			case 0xf3:	return {IndirectIndexedModify, Operation::INS};

			case 0x14:	return {ZeroXRead, Operation::NOP};
			case 0x34:	return {ZeroXRead, Operation::NOP};
			case 0x54:	return {ZeroXRead, Operation::NOP};
			case 0x74:	return {ZeroXRead, Operation::NOP};
			case 0x94:	return {ZeroXWrite, Operation::STY};
			case 0xb4:	return {ZeroXRead, Operation::LDY};
			case 0xd4:	return {ZeroXRead, Operation::NOP};
			case 0xf4:	return {ZeroXRead, Operation::NOP};

			case 0x15:	return {ZeroXRead, Operation::ORA};
			case 0x35:	return {ZeroXRead, Operation::AND};
			case 0x55:	return {ZeroXRead, Operation::EOR};
			case 0x75:	return {ZeroXRead, Operation::ADC};
			case 0x95:	return {ZeroXWrite, Operation::STA};
			case 0xb5:	return {ZeroXRead, Operation::LDA};
			case 0xd5:	return {ZeroXRead, Operation::CMP};
			case 0xf5:	return {ZeroXRead, Operation::SBC};

			case 0x16:	return {ZeroXModify, Operation::ASL};
			case 0x36:	return {ZeroXModify, Operation::ROL};
			case 0x56:	return {ZeroXModify, Operation::LSR};
			case 0x76:	return {ZeroXModify, Operation::ROR};
			case 0x96:	return {ZeroYWrite, Operation::STX};
			case 0xb6:	return {ZeroYRead, Operation::LDX};
			case 0xd6:	return {ZeroXModify, Operation::DEC};
			case 0xf6:	return {ZeroXModify, Operation::INC};

			case 0x17:	return {ZeroXModify, Operation::ASO};
			case 0x37:	return {ZeroXModify, Operation::RLA};
			case 0x57:	return {ZeroXModify, Operation::LSE};
			case 0x77:	return {ZeroXModify, Operation::RRA};
			case 0x97:	return {ZeroYWrite, Operation::SAX};
			case 0xb7:	return {ZeroYRead, Operation::LAX};
			case 0xd7:	return {ZeroXModify, Operation::DCP};
			case 0xf7:	return {ZeroXModify, Operation::INS};

			case 0x18:	return {Implied, Operation::CLC};
			case 0x38:	return {Implied, Operation::SEC};
			case 0x58:	return {Implied, Operation::CLI};
			case 0x78:	return {Implied, Operation::SEI};
			case 0x98:	return {Implied, Operation::TYA};
			case 0xb8:	return {Implied, Operation::CLV};
			case 0xd8:	return {Implied, Operation::CLD};
			case 0xf8:	return {Implied, Operation::SED};

			case 0x19:	return {AbsoluteYRead, Operation::ORA};
			case 0x39:	return {AbsoluteYRead, Operation::AND};
			case 0x59:	return {AbsoluteYRead, Operation::EOR};
			case 0x79:	return {AbsoluteYRead, Operation::ADC};
			case 0x99:	return {AbsoluteYWrite, Operation::STA};
			case 0xb9:	return {AbsoluteYRead, Operation::LDA};
			case 0xd9:	return {AbsoluteYRead, Operation::CMP};
			case 0xf9:	return {AbsoluteYRead, Operation::SBC};

			case 0x1a:	return {Implied, Operation::NOP};
			case 0x3a:	return {Implied, Operation::NOP};
			case 0x5a:	return {Implied, Operation::NOP};
			case 0x7a:	return {Implied, Operation::NOP};
			case 0x9a:	return {Implied, Operation::TXS};
			case 0xba:	return {Implied, Operation::TSX};
			case 0xda:	return {Implied, Operation::NOP};
			case 0xfa:	return {Implied, Operation::NOP};

			case 0x1b:	return {AbsoluteYModify, Operation::ASO};
			case 0x3b:	return {AbsoluteYModify, Operation::RLA};
			case 0x5b:	return {AbsoluteYModify, Operation::LSE};
			case 0x7b:	return {AbsoluteYModify, Operation::RRA};
			case 0x9b:	return {AbsoluteYWrite, Operation::SHS};
			case 0xbb:	return {AbsoluteYRead, Operation::LAS};
			case 0xdb:	return {AbsoluteYModify, Operation::DCP};
			case 0xfb:	return {AbsoluteYModify, Operation::INS};

			case 0x1c:	return {AbsoluteXNOP, Operation::NOP};
			case 0x3c:	return {AbsoluteXNOP, Operation::NOP};
			case 0x5c:	return {AbsoluteXNOP, Operation::NOP};
			case 0x7c:	return {AbsoluteXNOP, Operation::NOP};
			case 0x9c:	return {AbsoluteXWrite, Operation::SHY};
			case 0xbc:	return {AbsoluteXRead, Operation::LDY};
			case 0xdc:	return {AbsoluteXNOP, Operation::NOP};
			case 0xfc:	return {AbsoluteXNOP, Operation::NOP};

			case 0x1d:	return {AbsoluteXRead, Operation::ORA};
			case 0x3d:	return {AbsoluteXRead, Operation::AND};
			case 0x5d:	return {AbsoluteXRead, Operation::EOR};
			case 0x7d:	return {AbsoluteXRead, Operation::ADC};
			case 0x9d:	return {AbsoluteXWrite, Operation::STA};
			case 0xbd:	return {AbsoluteXRead, Operation::LDA};
			case 0xdd:	return {AbsoluteXRead, Operation::CMP};
			case 0xfd:	return {AbsoluteXRead, Operation::SBC};

			case 0x1e:	return {AbsoluteXModify, Operation::ASL};
			case 0x3e:	return {AbsoluteXModify, Operation::ROL};
			case 0x5e:	return {AbsoluteXModify, Operation::LSR};
			case 0x7e:	return {AbsoluteXModify, Operation::ROR};
			case 0x9e:	return {AbsoluteYWrite, Operation::SHX};
			case 0xbe:	return {AbsoluteYRead, Operation::LDX};
			case 0xde:	return {AbsoluteXModify, Operation::DEC};
			case 0xfe:	return {AbsoluteXModify, Operation::INC};

			case 0x1f:	return {AbsoluteXModify, Operation::ASO};
			case 0x3f:	return {AbsoluteXModify, Operation::RLA};
			case 0x5f:	return {AbsoluteXModify, Operation::LSE};
			case 0x7f:	return {AbsoluteXModify, Operation::ADC};
			case 0x9f:	return {AbsoluteYWrite, Operation::SHA};
			case 0xbf:	return {AbsoluteYRead, Operation::LAX};
			case 0xdf:	return {AbsoluteXModify, Operation::DCP};
			case 0xff:	return {AbsoluteXModify, Operation::INS};
		}

		__builtin_unreachable();
	}
};

}
