//
//  Z80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Disassembler_Z80_hpp
#define StaticAnalyser_Disassembler_Z80_hpp

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace StaticAnalyser {
namespace Z80 {

struct Instruction {
	/*! The address this instruction starts at. This is a mapped address. */
	uint16_t address = 0;

	/*! The operation this instruction performs. */
	enum class Operation {
		NOP,
		EXAFAFd, EXX, EX,
		LD, HALT,
		ADD, ADC, SUB, SBC, AND, XOR, OR, CP,
		INC, DEC,
		RLCA, RRCA, RLA, RRA, DAA, CPL, SCF, CCF,
		RLD, RRD,
		DJNZ, JR, JP, CALL, RST, RET, RETI, RETN,
		PUSH, POP,
		IN, OUT,
		EI, DI,
		RLC, RRC, RL, RR, SLA, SRA, SLL, SRL,
		BIT, RES, SET,
		LDI, CPI, INI, OUTI,
		LDD, CPD, IND, OUTD,
		LDIR, CPIR, INIR, OTIR,
		LDDR, CPDR, INDR, OTDR,
		NEG,
		IM,
		Invalid
	} operation = Operation::NOP;

	/*! The condition required for this instruction to take effect. */
	enum class Condition {
		None, NZ, Z, NC, C, PO, PE, P, M
	} condition = Condition::None;

	enum class Location {
		B, C, D, E, H, L, HL_Indirect, A, I, R,
		BC, DE, HL, SP, AF, Operand,
		IX_Indirect_Offset, IY_Indirect_Offset, IXh, IXl, IYh, IYl,
		Operand_Indirect,
		BC_Indirect, DE_Indirect, SP_Indirect,
		None
	};
	/*! The locations of source data for this instruction. */
	Location source = Location::None;
	/*! The locations of destination data from this instruction. */
	Location destination = Location::None;
	/*! The operand, if any; if this is used then it'll be referenced by either the source or destination location. */
	int operand = 0;
	/*! The offset to apply, if any; applies to IX_Indirect_Offset and IY_Indirect_Offset locations. */
	int offset = 0;
};

struct Disassembly {
	std::map<uint16_t, Instruction> instructions_by_address;
	std::set<uint16_t> outward_calls;
	std::set<uint16_t> internal_calls;
	std::set<uint16_t> external_stores, external_loads, external_modifies;
	std::set<uint16_t> internal_stores, internal_loads, internal_modifies;
};

Disassembly Disassemble(
	const std::vector<uint8_t> &memory,
	const std::function<std::size_t(uint16_t)> &address_mapper,
	std::vector<uint16_t> entry_points);

}
}

#endif /* StaticAnalyser_Disassembler_Z80_hpp */
