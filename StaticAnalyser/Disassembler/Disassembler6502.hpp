//
//  Disassembler6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Disassembler6502_hpp
#define Disassembler6502_hpp

#include <cstdint>
#include <memory>
#include <set>
#include <vector>
#include <map>

namespace StaticAnalyser {
namespace MOS6502 {

struct Instruction {
	uint16_t address;
	enum {
		BRK, JSR, RTI, RTS, JMP,
		CLC, SEC, CLD, SED, CLI, SEI, CLV,
		NOP,

		SLO, RLA, SRE, RRA, ALR, ARR,
		SAX, LAX, DCP, ISC,
		ANC, XAA, AXS,
		AND, EOR, ORA, BIT,
		ADC, SBC,
		AHX, SHY, SHX, TAS, LAS,

		LDA, STA, LDX, STX, LDY, STY,

		BPL, BMI, BVC, BVS, BCC, BCS, BNE, BEQ,

		CMP, CPX, CPY,
		INC, DEC, DEX, DEY, INX, INY,
		ASL, ROL, LSR, ROR,
		TAX, TXA, TAY, TYA, TSX, TXS,
		PLA, PHA, PLP, PHP,

		KIL
	} operation;
	enum {
		Absolute,
		AbsoluteX,
		AbsoluteY,
		Immediate,
		Implied,
		ZeroPage,
		ZeroPageX,
		ZeroPageY,
		Indirect,
		IndexedIndirectX,
		IndirectIndexedY,
		Relative,
	} addressing_mode;
	uint16_t operand;
};

struct Disassembly {
	std::map<uint16_t, Instruction> instructions_by_address;
	std::set<uint16_t> outward_calls;
	std::set<uint16_t> external_stores, external_loads, external_modifies;
};

Disassembly Disassemble(const std::vector<uint8_t> &memory, uint16_t start_address, std::vector<uint16_t> entry_points);

}
}

#endif /* Disassembler6502_hpp */
