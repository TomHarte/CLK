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
	NOP,	FastNOP,

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
	JAM,
};

enum class AddressingMode {
	Implied,
	Immediate,
	Accumulator,
	Relative,

	Push,
	Pull,

	Absolute,
	AbsoluteIndexed,
	Zero,
	ZeroIndexed,
	ZeroIndirect,
	IndexedIndirect,
	IndirectIndexed,

	// Irregular flow control.
	BRK,	JSR,	RTI,	RTS,
	JMPAbsolute,	JMPAbsoluteIndirect,

	// Irregular unintended, undocumented and unreliable.
	SHxIndirectIndexed,
	SHxAbsoluteXY,

	// Terminal.
	JAM,

	Max,
};

enum class Index {
	X, Y
};

constexpr Index index_of(const Operation operation) {
	switch(operation) {
		default: return Index::X;

		case Operation::STX:	case Operation::LDX:
		case Operation::SAX:	case Operation::LAX:
			return Index::Y;
	}
}

enum class Type {
	Read, Modify, Write
};

constexpr Type type_of(const Operation operation) {
	switch(operation) {
		// All of these don't really fit the type orthodoxy.
		case Operation::BRK:	case Operation::JAM:
		case Operation::SHA:	case Operation::SHX:	case Operation::SHY:
		case Operation::SHS:	case Operation::CLC:	case Operation::CLI:
		case Operation::CLV:	case Operation::CLD:	case Operation::SEC:
		case Operation::SEI:	case Operation::SED:
		case Operation::INX:	case Operation::DEX:	case Operation::INY:
		case Operation::DEY:	case Operation::INA:	case Operation::DEA:
		case Operation::BPL:	case Operation::BMI:	case Operation::BVC:
		case Operation::BVS:	case Operation::BCC:	case Operation::BCS:
		case Operation::BNE:	case Operation::BEQ:	case Operation::BRA:
		case Operation::TXA:	case Operation::TYA:	case Operation::TXS:
		case Operation::TAY:	case Operation::TAX:	case Operation::TSX:
		case Operation::JSR:	case Operation::RTI:	case Operation::RTS:
		case Operation::PHP:	case Operation::PLP:	case Operation::JMP:
		case Operation::BBRBBS:
			return Type::Modify;

		case Operation::ORA:	case Operation::AND:	case Operation::EOR:
		case Operation::ADC:	case Operation::SBC:
		case Operation::CMP:	case Operation::CPX:	case Operation::CPY:
		case Operation::BIT:	case Operation::BITNoNV:
		case Operation::LDA:	case Operation::LDX:
		case Operation::LDY:	case Operation::LAX:
		case Operation::ARR:	case Operation::SBX:	case Operation::LXA:
		case Operation::ANE:	case Operation::ANC:	case Operation::LAS:
		case Operation::NOP:	case Operation::FastNOP:
			return Type::Read;

		case Operation::STA:	case Operation::STX:	case Operation::STY:
		case Operation::STZ:	case Operation::SAX:
			return Type::Write;

		case Operation::INS:
		case Operation::ASL:	case Operation::ASO:	case Operation::ROL:
		case Operation::RLA:	case Operation::LSR:	case Operation::LSE:
		case Operation::ASR:	case Operation::ROR:	case Operation::RRA:
		case Operation::RMB:	case Operation::SMB:
		case Operation::TRB:	case Operation::TSB:
		case Operation::INC:	case Operation::DEC:	case Operation::DCP:
			return Type::Modify;
	}
	return Type::Read;
}

struct Instruction {
	consteval Instruction(const AddressingMode mode, const Operation operation) noexcept :
		operation(operation), mode(mode), index(index_of(operation)), type(type_of(operation)) {}
	consteval Instruction(const AddressingMode mode, const Index index, const Operation operation) noexcept :
		operation(operation), mode(mode), index(index), type(type_of(operation)) {}
	Instruction() = default;

	Operation operation;
	AddressingMode mode;
	Index index;
	Type type;
};

template <Model model, typename Enable = void> struct Decoder;

template <Model model>
struct Decoder<model, std::enable_if_t<is_6502(model)>> {
	static constexpr Instruction decode(const uint8_t opcode) {
		using enum AddressingMode;
		switch(opcode) {
			case 0x00:	return {BRK, Operation::BRK};
			case 0x20:	return {JSR, Operation::JSR};
			case 0x40:	return {RTI, Operation::RTI};
			case 0x60:	return {RTS, Operation::RTS};
			case 0x80:	return {Immediate, Operation::NOP};
			case 0xa0:	return {Immediate, Operation::LDY};
			case 0xc0:	return {Immediate, Operation::CPY};
			case 0xe0:	return {Immediate, Operation::CPX};

			case 0x01:	return {IndexedIndirect, Operation::ORA};
			case 0x21:	return {IndexedIndirect, Operation::AND};
			case 0x41:	return {IndexedIndirect, Operation::EOR};
			case 0x61:	return {IndexedIndirect, Operation::ADC};
			case 0x81:	return {IndexedIndirect, Operation::STA};
			case 0xa1:	return {IndexedIndirect, Operation::LDA};
			case 0xc1:	return {IndexedIndirect, Operation::CMP};
			case 0xe1:	return {IndexedIndirect, Operation::SBC};

			case 0x02:	return {JAM, Operation::JAM};
			case 0x22:	return {JAM, Operation::JAM};
			case 0x42:	return {JAM, Operation::JAM};
			case 0x62:	return {JAM, Operation::JAM};
			case 0x82:	return {Immediate, Operation::NOP};
			case 0xa2:	return {Immediate, Operation::LDX};
			case 0xc2:	return {Immediate, Operation::NOP};
			case 0xe2:	return {Immediate, Operation::NOP};

			case 0x03:	return {IndexedIndirect, Operation::ASO};
			case 0x23:	return {IndexedIndirect, Operation::RLA};
			case 0x43:	return {IndexedIndirect, Operation::LSE};
			case 0x63:	return {IndexedIndirect, Operation::RRA};
			case 0x83:	return {IndexedIndirect, Operation::SAX};
			case 0xa3:	return {IndexedIndirect, Operation::LAX};
			case 0xc3:	return {IndexedIndirect, Operation::DCP};
			case 0xe3:	return {IndexedIndirect, Operation::INS};

			case 0x04:	return {Zero, Operation::NOP};
			case 0x24:	return {Zero, Operation::BIT};
			case 0x44:	return {Zero, Operation::NOP};
			case 0x64:	return {Zero, Operation::NOP};
			case 0x84:	return {Zero, Operation::STY};
			case 0xa4:	return {Zero, Operation::LDY};
			case 0xc4:	return {Zero, Operation::CPY};
			case 0xe4:	return {Zero, Operation::CPX};

			case 0x05:	return {Zero, Operation::ORA};
			case 0x25:	return {Zero, Operation::AND};
			case 0x45:	return {Zero, Operation::EOR};
			case 0x65:	return {Zero, Operation::ADC};
			case 0x85:	return {Zero, Operation::STA};
			case 0xa5:	return {Zero, Operation::LDA};
			case 0xc5:	return {Zero, Operation::CMP};
			case 0xe5:	return {Zero, Operation::SBC};

			case 0x06:	return {Zero, Operation::ASL};
			case 0x26:	return {Zero, Operation::ROL};
			case 0x46:	return {Zero, Operation::LSR};
			case 0x66:	return {Zero, Operation::ROR};
			case 0x86:	return {Zero, Operation::STX};
			case 0xa6:	return {Zero, Operation::LDX};
			case 0xc6:	return {Zero, Operation::DEC};
			case 0xe6:	return {Zero, Operation::INC};

			case 0x07:	return {Zero, Operation::ASO};
			case 0x27:	return {Zero, Operation::RLA};
			case 0x47:	return {Zero, Operation::LSE};
			case 0x67:	return {Zero, Operation::RRA};
			case 0x87:	return {Zero, Operation::SAX};
			case 0xa7:	return {Zero, Operation::LAX};
			case 0xc7:	return {Zero, Operation::DCP};
			case 0xe7:	return {Zero, Operation::INS};

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

			case 0x0c:	return {Absolute, Operation::NOP};
			case 0x2c:	return {Absolute, Operation::BIT};
			case 0x4c:	return {JMPAbsolute, Operation::JMP};
			case 0x6c:	return {JMPAbsoluteIndirect, Operation::JMP};
			case 0x8c:	return {Absolute, Operation::STY};
			case 0xac:	return {Absolute, Operation::LDY};
			case 0xcc:	return {Absolute, Operation::CPY};
			case 0xec:	return {Absolute, Operation::CPX};

			case 0x0d:	return {Absolute, Operation::ORA};
			case 0x2d:	return {Absolute, Operation::AND};
			case 0x4d:	return {Absolute, Operation::EOR};
			case 0x6d:	return {Absolute, Operation::ADC};
			case 0x8d:	return {Absolute, Operation::STA};
			case 0xad:	return {Absolute, Operation::LDA};
			case 0xcd:	return {Absolute, Operation::CMP};
			case 0xed:	return {Absolute, Operation::SBC};

			case 0x0e:	return {Absolute, Operation::ASL};
			case 0x2e:	return {Absolute, Operation::ROL};
			case 0x4e:	return {Absolute, Operation::LSR};
			case 0x6e:	return {Absolute, Operation::ROR};
			case 0x8e:	return {Absolute, Operation::STX};
			case 0xae:	return {Absolute, Operation::LDX};
			case 0xce:	return {Absolute, Operation::DEC};
			case 0xee:	return {Absolute, Operation::INC};

			case 0x0f:	return {Absolute, Operation::ASO};
			case 0x2f:	return {Absolute, Operation::RLA};
			case 0x4f:	return {Absolute, Operation::LSE};
			case 0x6f:	return {Absolute, Operation::RRA};
			case 0x8f:	return {Absolute, Operation::SAX};
			case 0xaf:	return {Absolute, Operation::LAX};
			case 0xcf:	return {Absolute, Operation::DCP};
			case 0xef:	return {Absolute, Operation::INS};

			case 0x10:	return {Relative, Operation::BPL};
			case 0x30:	return {Relative, Operation::BMI};
			case 0x50:	return {Relative, Operation::BVC};
			case 0x70:	return {Relative, Operation::BVS};
			case 0x90:	return {Relative, Operation::BCC};
			case 0xb0:	return {Relative, Operation::BCS};
			case 0xd0:	return {Relative, Operation::BNE};
			case 0xf0:	return {Relative, Operation::BEQ};

			case 0x11:	return {IndirectIndexed, Operation::ORA};
			case 0x31:	return {IndirectIndexed, Operation::AND};
			case 0x51:	return {IndirectIndexed, Operation::EOR};
			case 0x71:	return {IndirectIndexed, Operation::ADC};
			case 0x91:	return {IndirectIndexed, Operation::STA};
			case 0xb1:	return {IndirectIndexed, Operation::LDA};
			case 0xd1:	return {IndirectIndexed, Operation::CMP};
			case 0xf1:	return {IndirectIndexed, Operation::SBC};

			case 0x12:	return {JAM, Operation::JAM};
			case 0x32:	return {JAM, Operation::JAM};
			case 0x52:	return {JAM, Operation::JAM};
			case 0x72:	return {JAM, Operation::JAM};
			case 0x92:	return {JAM, Operation::JAM};
			case 0xb2:	return {JAM, Operation::JAM};
			case 0xd2:	return {JAM, Operation::JAM};
			case 0xf2:	return {JAM, Operation::JAM};

			case 0x13:	return {IndirectIndexed, Operation::ASO};
			case 0x33:	return {IndirectIndexed, Operation::RLA};
			case 0x53:	return {IndirectIndexed, Operation::LSE};
			case 0x73:	return {IndirectIndexed, Operation::RRA};
			case 0x93:	return {SHxIndirectIndexed, Operation::SHA};
			case 0xb3:	return {IndirectIndexed, Operation::LAX};
			case 0xd3:	return {IndirectIndexed, Operation::DCP};
			case 0xf3:	return {IndirectIndexed, Operation::INS};

			case 0x14:	return {ZeroIndexed, Operation::NOP};
			case 0x34:	return {ZeroIndexed, Operation::NOP};
			case 0x54:	return {ZeroIndexed, Operation::NOP};
			case 0x74:	return {ZeroIndexed, Operation::NOP};
			case 0x94:	return {ZeroIndexed, Operation::STY};
			case 0xb4:	return {ZeroIndexed, Operation::LDY};
			case 0xd4:	return {ZeroIndexed, Operation::NOP};
			case 0xf4:	return {ZeroIndexed, Operation::NOP};

			case 0x15:	return {ZeroIndexed, Operation::ORA};
			case 0x35:	return {ZeroIndexed, Operation::AND};
			case 0x55:	return {ZeroIndexed, Operation::EOR};
			case 0x75:	return {ZeroIndexed, Operation::ADC};
			case 0x95:	return {ZeroIndexed, Operation::STA};
			case 0xb5:	return {ZeroIndexed, Operation::LDA};
			case 0xd5:	return {ZeroIndexed, Operation::CMP};
			case 0xf5:	return {ZeroIndexed, Operation::SBC};

			case 0x16:	return {ZeroIndexed, Operation::ASL};
			case 0x36:	return {ZeroIndexed, Operation::ROL};
			case 0x56:	return {ZeroIndexed, Operation::LSR};
			case 0x76:	return {ZeroIndexed, Operation::ROR};
			case 0x96:	return {ZeroIndexed, Operation::STX};
			case 0xb6:	return {ZeroIndexed, Operation::LDX};
			case 0xd6:	return {ZeroIndexed, Operation::DEC};
			case 0xf6:	return {ZeroIndexed, Operation::INC};

			case 0x17:	return {ZeroIndexed, Operation::ASO};
			case 0x37:	return {ZeroIndexed, Operation::RLA};
			case 0x57:	return {ZeroIndexed, Operation::LSE};
			case 0x77:	return {ZeroIndexed, Operation::RRA};
			case 0x97:	return {ZeroIndexed, Operation::SAX};
			case 0xb7:	return {ZeroIndexed, Operation::LAX};
			case 0xd7:	return {ZeroIndexed, Operation::DCP};
			case 0xf7:	return {ZeroIndexed, Operation::INS};

			case 0x18:	return {Implied, Operation::CLC};
			case 0x38:	return {Implied, Operation::SEC};
			case 0x58:	return {Implied, Operation::CLI};
			case 0x78:	return {Implied, Operation::SEI};
			case 0x98:	return {Implied, Operation::TYA};
			case 0xb8:	return {Implied, Operation::CLV};
			case 0xd8:	return {Implied, Operation::CLD};
			case 0xf8:	return {Implied, Operation::SED};

			case 0x19:	return {AbsoluteIndexed, Index::Y, Operation::ORA};
			case 0x39:	return {AbsoluteIndexed, Index::Y, Operation::AND};
			case 0x59:	return {AbsoluteIndexed, Index::Y, Operation::EOR};
			case 0x79:	return {AbsoluteIndexed, Index::Y, Operation::ADC};
			case 0x99:	return {AbsoluteIndexed, Index::Y, Operation::STA};
			case 0xb9:	return {AbsoluteIndexed, Index::Y, Operation::LDA};
			case 0xd9:	return {AbsoluteIndexed, Index::Y, Operation::CMP};
			case 0xf9:	return {AbsoluteIndexed, Index::Y, Operation::SBC};

			case 0x1a:	return {Implied, Operation::NOP};
			case 0x3a:	return {Implied, Operation::NOP};
			case 0x5a:	return {Implied, Operation::NOP};
			case 0x7a:	return {Implied, Operation::NOP};
			case 0x9a:	return {Implied, Operation::TXS};
			case 0xba:	return {Implied, Operation::TSX};
			case 0xda:	return {Implied, Operation::NOP};
			case 0xfa:	return {Implied, Operation::NOP};

			case 0x1b:	return {AbsoluteIndexed, Index::Y, Operation::ASO};
			case 0x3b:	return {AbsoluteIndexed, Index::Y, Operation::RLA};
			case 0x5b:	return {AbsoluteIndexed, Index::Y, Operation::LSE};
			case 0x7b:	return {AbsoluteIndexed, Index::Y, Operation::RRA};
			case 0x9b:	return {SHxAbsoluteXY, Operation::SHS};
			case 0xbb:	return {AbsoluteIndexed, Index::Y, Operation::LAS};
			case 0xdb:	return {AbsoluteIndexed, Index::Y, Operation::DCP};
			case 0xfb:	return {AbsoluteIndexed, Index::Y, Operation::INS};

			case 0x1c:	return {AbsoluteIndexed, Index::X, Operation::NOP};
			case 0x3c:	return {AbsoluteIndexed, Index::X, Operation::NOP};
			case 0x5c:	return {AbsoluteIndexed, Index::X, Operation::NOP};
			case 0x7c:	return {AbsoluteIndexed, Index::X, Operation::NOP};
			case 0x9c:	return {SHxAbsoluteXY, Operation::SHY};
			case 0xbc:	return {AbsoluteIndexed, Index::X, Operation::LDY};
			case 0xdc:	return {AbsoluteIndexed, Index::X, Operation::NOP};
			case 0xfc:	return {AbsoluteIndexed, Index::X, Operation::NOP};

			case 0x1d:	return {AbsoluteIndexed, Index::X, Operation::ORA};
			case 0x3d:	return {AbsoluteIndexed, Index::X, Operation::AND};
			case 0x5d:	return {AbsoluteIndexed, Index::X, Operation::EOR};
			case 0x7d:	return {AbsoluteIndexed, Index::X, Operation::ADC};
			case 0x9d:	return {AbsoluteIndexed, Index::X, Operation::STA};
			case 0xbd:	return {AbsoluteIndexed, Index::X, Operation::LDA};
			case 0xdd:	return {AbsoluteIndexed, Index::X, Operation::CMP};
			case 0xfd:	return {AbsoluteIndexed, Index::X, Operation::SBC};

			case 0x1e:	return {AbsoluteIndexed, Operation::ASL};
			case 0x3e:	return {AbsoluteIndexed, Operation::ROL};
			case 0x5e:	return {AbsoluteIndexed, Operation::LSR};
			case 0x7e:	return {AbsoluteIndexed, Operation::ROR};
			case 0x9e:	return {SHxAbsoluteXY, Operation::SHX};
			case 0xbe:	return {AbsoluteIndexed, Operation::LDX};
			case 0xde:	return {AbsoluteIndexed, Operation::DEC};
			case 0xfe:	return {AbsoluteIndexed, Operation::INC};

			case 0x1f:	return {AbsoluteIndexed, Operation::ASO};
			case 0x3f:	return {AbsoluteIndexed, Operation::RLA};
			case 0x5f:	return {AbsoluteIndexed, Operation::LSE};
			case 0x7f:	return {AbsoluteIndexed, Operation::RRA};
			case 0x9f:	return {SHxAbsoluteXY, Operation::SHA};
			case 0xbf:	return {AbsoluteIndexed, Operation::LAX};
			case 0xdf:	return {AbsoluteIndexed, Operation::DCP};
			case 0xff:	return {AbsoluteIndexed, Operation::INS};
		}

		__builtin_unreachable();
	}
};

template <Model model>
struct Decoder<model, std::enable_if_t<model == Model::Synertek65C02>> {
	static constexpr Instruction decode(const uint8_t opcode) {
		using enum AddressingMode;
		switch(opcode) {
			default: return Decoder<Model::M6502>::decode(opcode);

			case 0x80:	return {Relative, Operation::BRA};

			case 0x02:	return {Immediate, Operation::NOP};
			case 0x22:	return {Immediate, Operation::NOP};
			case 0x42:	return {Immediate, Operation::NOP};
			case 0x62:	return {Immediate, Operation::NOP};

			case 0x03:	return {Implied, Operation::FastNOP};
			case 0x23:	return {Implied, Operation::FastNOP};
			case 0x43:	return {Implied, Operation::FastNOP};
			case 0x63:	return {Implied, Operation::FastNOP};
			case 0x83:	return {Implied, Operation::FastNOP};
			case 0xa3:	return {Implied, Operation::FastNOP};
			case 0xc3:	return {Implied, Operation::FastNOP};
			case 0xe3:	return {Implied, Operation::FastNOP};

			case 0x04:	return {Zero, Operation::TSB};
			case 0x64:	return {Zero, Operation::STZ};
			case 0x9e:	return {AbsoluteIndexed, Operation::STZ};

			case 0x07:	return {Zero, Operation::NOP};
			case 0x27:	return {Zero, Operation::NOP};
			case 0x47:	return {Zero, Operation::NOP};
			case 0x67:	return {Zero, Operation::NOP};
			case 0x87:	return {Zero, Operation::NOP};
			case 0xa7:	return {Zero, Operation::NOP};
			case 0xc7:	return {Zero, Operation::NOP};
			case 0xe7:	return {Zero, Operation::NOP};

			case 0x89:	return {Immediate, Operation::BITNoNV};

			case 0x0b:	return {Implied, Operation::FastNOP};
			case 0x2b:	return {Implied, Operation::FastNOP};
			case 0x4b:	return {Implied, Operation::FastNOP};
			case 0x6b:	return {Implied, Operation::FastNOP};
			case 0x8b:	return {Implied, Operation::FastNOP};
			case 0xab:	return {Implied, Operation::FastNOP};
			case 0xcb:	return {Implied, Operation::FastNOP};
			case 0xeb:	return {Implied, Operation::FastNOP};

			case 0x0c:	return {Absolute, Operation::TSB};

			case 0x0f:	return {Absolute, Operation::FastNOP};
			case 0x2f:	return {Absolute, Operation::FastNOP};
			case 0x4f:	return {Absolute, Operation::FastNOP};
			case 0x6f:	return {Absolute, Operation::FastNOP};
			case 0x8f:	return {Absolute, Operation::FastNOP};
			case 0xaf:	return {Absolute, Operation::FastNOP};
			case 0xcf:	return {Absolute, Operation::FastNOP};
			case 0xef:	return {Absolute, Operation::FastNOP};

			case 0x12:	return {ZeroIndirect, Operation::ORA};
			case 0x32:	return {ZeroIndirect, Operation::AND};
			case 0x52:	return {ZeroIndirect, Operation::EOR};
			case 0x72:	return {ZeroIndirect, Operation::ADC};
			case 0x92:	return {ZeroIndirect, Operation::STA};
			case 0xb2:	return {ZeroIndirect, Operation::LDA};
			case 0xd2:	return {ZeroIndirect, Operation::CMP};
			case 0xf2:	return {ZeroIndirect, Operation::SBC};

			case 0x13:	return {Implied, Operation::FastNOP};
			case 0x33:	return {Implied, Operation::FastNOP};
			case 0x53:	return {Implied, Operation::FastNOP};
			case 0x73:	return {Implied, Operation::FastNOP};
			case 0x93:	return {Implied, Operation::FastNOP};
			case 0xb3:	return {Implied, Operation::FastNOP};
			case 0xd3:	return {Implied, Operation::FastNOP};
			case 0xf3:	return {Implied, Operation::FastNOP};

			case 0x14:	return {Zero, Operation::TRB};
			case 0x34:	return {ZeroIndexed, Operation::BIT};
			case 0x74:	return {ZeroIndexed, Operation::STZ};

			case 0x17:	return {ZeroIndexed, Operation::NOP};
			case 0x37:	return {ZeroIndexed, Operation::NOP};
			case 0x57:	return {ZeroIndexed, Operation::NOP};
			case 0x77:	return {ZeroIndexed, Operation::NOP};
			case 0x97:	return {ZeroIndexed, Operation::NOP};
			case 0xb7:	return {ZeroIndexed, Operation::NOP};
			case 0xd7:	return {ZeroIndexed, Operation::NOP};
			case 0xf7:	return {ZeroIndexed, Operation::NOP};

			case 0x1a:	return {Implied, Operation::INA};
			case 0x3a:	return {Implied, Operation::DEA};
			case 0x5a:	return {Push, Operation::STY};
			case 0x7a:	return {Pull, Operation::LDY};
			case 0xda:	return {Push, Operation::STX};
			case 0xfa:	return {Push, Operation::LDX};

			case 0x1b:	return {Implied, Operation::NOP};
			case 0x3b:	return {Implied, Operation::NOP};
			case 0x5b:	return {Implied, Operation::NOP};
			case 0x7b:	return {Implied, Operation::NOP};
			case 0x9b:	return {Implied, Operation::NOP};
			case 0xbb:	return {Implied, Operation::NOP};
			case 0xdb:	return {ZeroIndexed, Operation::NOP};
			case 0xfb:	return {Implied, Operation::NOP};

			case 0x1c:	return {Absolute, Operation::TRB};
			case 0x3c:	return {AbsoluteIndexed, Operation::BIT};
			case 0x9c:	return {Absolute, Operation::STZ};

			case 0x1f:	return {AbsoluteIndexed, Operation::NOP};
			case 0x3f:	return {AbsoluteIndexed, Operation::NOP};
			case 0x5f:	return {AbsoluteIndexed, Operation::NOP};
			case 0x7f:	return {AbsoluteIndexed, Operation::NOP};
			case 0x9f:	return {AbsoluteIndexed, Operation::NOP};
			case 0xbf:	return {AbsoluteIndexed, Operation::NOP};
			case 0xdf:	return {AbsoluteIndexed, Operation::NOP};
			case 0xff:	return {AbsoluteIndexed, Operation::NOP};
		}
	}
};

}
