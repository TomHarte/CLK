//
//  Disassembler6502.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disassembler6502.hpp"

using namespace StaticAnalyser::MOS6502;

static void AddToDisassembly(const std::unique_ptr<Disassembly> &disassembly, uint16_t start_address, uint16_t entry_point)
{
}

std::unique_ptr<Disassembly> Disassemble(std::vector<uint8_t> memory, uint16_t start_address, std::vector<uint16_t> entry_points)
{
	std::unique_ptr<Disassembly> disassembly;



	return disassembly;
}
