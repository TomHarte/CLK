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

namespace StaticAnalyser {
namespace MOS6502 {

struct Instruction {
	uint16_t address;
	enum {
		BRK, ORA, KIL, SLO, NOP, ASL, PHP, ANC, BPL
	} operation;
	enum {
		Absolute, AbsoluteIndirect, Accumulator,
		Immediate, Implied, IndexAbsolute, IndexedZeroPage,
		IndexedIndirectX, IndirectIndexedY, Relative, ZeroPage
	} addressing_mode;
	uint16_t operand;
};

struct Disassembly {
	std::vector<Instruction> instructions;
	std::set<uint16_t> outward_calls;
};

std::unique_ptr<Disassembly> Disassemble(std::vector<uint8_t> memory, uint16_t start_address, std::vector<uint16_t> entry_points);

}
}

#endif /* Disassembler6502_hpp */
