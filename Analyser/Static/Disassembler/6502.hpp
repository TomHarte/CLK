//
//  6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Disassembler_6502_hpp
#define StaticAnalyser_Disassembler_6502_hpp

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace Analyser {
namespace Static {
namespace MOS6502 {

/*!
	Describes a 6502 instruciton — its address, the operation it performs, its addressing mode
	and its operand, if any.
*/
struct Instruction {
	/*! The address this instruction starts at. This is a mapped address. */
	uint16_t address = 0;
	/*! The operation this instruction performs. */
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
	} operation = NOP;
	/*! The addressing mode used by the instruction. */
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
	} addressing_mode = Implied;
	/*! The instruction's operand, if any. */
	uint16_t operand = 0;
};

/*! Represents the disassembled form of a program. */
struct Disassembly {
	/*! All instructions found, mapped by address. */
	std::map<uint16_t, Instruction> instructions_by_address;
	/*! The set of all calls or jumps that land outside of the area covered by the data provided for disassembly. */
	std::set<uint16_t> outward_calls;
	/*! The set of all calls or jumps that land inside of the area covered by the data provided for disassembly. */
	std::set<uint16_t> internal_calls;
	/*! The sets of all stores, loads and modifies that occur to data outside of the area covered by the data provided for disassembly. */
	std::set<uint16_t> external_stores, external_loads, external_modifies;
	/*! The sets of all stores, loads and modifies that occur to data inside of the area covered by the data provided for disassembly. */
	std::set<uint16_t> internal_stores, internal_loads, internal_modifies;
};

/*!
	Disassembles the data provided as @c memory, mapping it into the 6502's full address range via the @c address_mapper,
	starting disassembly from each of the @c entry_points.
*/
Disassembly Disassemble(
	const std::vector<uint8_t> &memory,
	const std::function<std::size_t(uint16_t)> &address_mapper,
	std::vector<uint16_t> entry_points);

}
}
}

#endif /* Disassembler6502_hpp */
