//
//  Disassembler6502.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disassembler6502.hpp"
#include <map>

using namespace StaticAnalyser::MOS6502;

struct PartialDisassembly {
	Disassembly disassembly;
	std::vector<uint16_t> remaining_entry_points;
};


static void AddToDisassembly(PartialDisassembly &disassembly, const std::vector<uint8_t> &memory, uint16_t start_address, uint16_t entry_point)
{
	uint16_t address = entry_point;
	while(1)
	{
		uint16_t local_address = address - start_address;
		if(local_address > memory.size()) return;
		address++;

		struct Instruction instruction;
		instruction.address = address;

		// get operation
		uint8_t operation = memory[local_address];

		// decode addressing mode
		switch(operation&0x1f)
		{
			case 0x00:
				if(operation >= 0x80) instruction.addressing_mode = Instruction::Immediate;
				else if(operation == 0x20) instruction.addressing_mode = Instruction::Absolute;
				else instruction.addressing_mode = Instruction::Implied;
			break;
			case 0x08:	case 0x18:	case 0x0a:	case 0x1a:	case 0x12:
				instruction.addressing_mode = Instruction::Implied;
			break;
			case 0x10:
				instruction.addressing_mode = Instruction::Relative;
			break;
			case 0x01: case 0x03:
				instruction.addressing_mode = Instruction::IndexedIndirectX;
			break;
			case 0x02:	case 0x09:	case 0x0b:
				instruction.addressing_mode = Instruction::Immediate;
			break;
			case 0x04:	case 0x05:	case 0x06:	case 0x07:
				instruction.addressing_mode = Instruction::ZeroPage;
			break;
			case 0x0c:	case 0x0d:	case 0x0e:	case 0x0f:
				instruction.addressing_mode = (operation == 0x6c) ? Instruction::Indirect : Instruction::Absolute;
			break;
			case 0x11:	case 0x13:
				instruction.addressing_mode = Instruction::IndirectIndexedY;
			break;
			case 0x14:	case 0x15:	case 0x16:	case 0x17:
				instruction.addressing_mode =
					(operation == 0x96 || operation == 0xb6 || operation == 0x97 || operation == 0xb7)
						? Instruction::ZeroPageY : Instruction::ZeroPageX;
			break;
			case 0x19:	case 0x1b:
				instruction.addressing_mode = Instruction::AbsoluteY;
			break;
			case 0x1c:	case 0x1d:	case 0x1e: case 0x1f:
				instruction.addressing_mode =
					(operation == 0x9e || operation == 0xbe || operation == 0x9f || operation == 0xbf)
						? Instruction::AbsoluteY : Instruction::AbsoluteX;
			break;
		}

		// decode operation
#define RM_INSTRUCTION(base, op)	\
	case base+0x09: case base+0x05: case base+0x15: case base+0x01: case base+0x11: case base+0x0d: case base+0x1d: case base+0x19:	\
		instruction.operation = op;	\
	break;

#define M_INSTRUCTION(base, op)	\
	case base+0x0a: case base+0x06: case base+0x16: case base+0x0e: case base+0x1e:	\
		instruction.operation = op;	\
	break;

#define IM_INSTRUCTION(base, op)	\
	case base:	instruction.operation = op; break;
		switch(operation)
		{
			IM_INSTRUCTION(0x00, Instruction::BRK)
			IM_INSTRUCTION(0x40, Instruction::RTI)
			IM_INSTRUCTION(0x60, Instruction::RTS)

			IM_INSTRUCTION(0xca, Instruction::DEX)
			IM_INSTRUCTION(0x88, Instruction::DEY)
			IM_INSTRUCTION(0xe8, Instruction::INX)
			IM_INSTRUCTION(0xc8, Instruction::INY)

			IM_INSTRUCTION(0xaa, Instruction::TAX)
			IM_INSTRUCTION(0x8a, Instruction::TXA)
			IM_INSTRUCTION(0xa8, Instruction::TAY)
			IM_INSTRUCTION(0x98, Instruction::TYA)
			IM_INSTRUCTION(0xba, Instruction::TSX)
			IM_INSTRUCTION(0x9a, Instruction::TXS)

			IM_INSTRUCTION(0x68, Instruction::PLA)
			IM_INSTRUCTION(0x48, Instruction::PHA)
			IM_INSTRUCTION(0x28, Instruction::PLP)
			IM_INSTRUCTION(0x08, Instruction::PHP)

			IM_INSTRUCTION(0x18, Instruction::CLC)
			IM_INSTRUCTION(0x38, Instruction::SEC)
			IM_INSTRUCTION(0xd8, Instruction::CLD)
			IM_INSTRUCTION(0xf8, Instruction::SED)
			IM_INSTRUCTION(0x58, Instruction::CLI)
			IM_INSTRUCTION(0x78, Instruction::SEI)
			IM_INSTRUCTION(0xb8, Instruction::CLV)

			RM_INSTRUCTION(0x00, Instruction::ORA)
			RM_INSTRUCTION(0x20, Instruction::AND)
			RM_INSTRUCTION(0x40, Instruction::EOR)
			RM_INSTRUCTION(0x60, Instruction::ADC)
			RM_INSTRUCTION(0xc0, Instruction::CMP)
			RM_INSTRUCTION(0xe0, Instruction::SBC)

			M_INSTRUCTION(0x00, Instruction::ASL)
			M_INSTRUCTION(0x20, Instruction::ROL)
			M_INSTRUCTION(0x40, Instruction::LSR)
			M_INSTRUCTION(0x60, Instruction::ROR)

			case 0xe0: case 0xe4: case 0xec:			instruction.operation =	Instruction::CPX;	break;
			case 0xc0: case 0xc4: case 0xcc:			instruction.operation = Instruction::CPY;	break;
			case 0xc6: case 0xd6: case 0xce: case 0xde:	instruction.operation = Instruction::DEC;	break;
			case 0xe6: case 0xf6: case 0xee: case 0xfe:	instruction.operation = Instruction::INC;	break;

			RM_INSTRUCTION(0xa0, Instruction::LDA)
			case 0x85: case 0x95: case 0x81: case 0x91: case 0x8d: case 0x9d: case 0x99:
				instruction.operation = Instruction::STA;
			break;
			case 0xa2: case 0xa6: case 0xb6: case 0xae: case 0xbe:
				instruction.operation = Instruction::LDX;
			break;
			case 0x86: case 0x96: case 0x8e:
				instruction.operation = Instruction::STX;
			break;
			case 0xa0: case 0xa4: case 0xb4: case 0xac: case 0xbc:
				instruction.operation = Instruction::LDY;
			break;
			case 0x84: case 0x94: case 0x8c:
				instruction.operation = Instruction::STY;
			break;
			
		}
		disassembly.disassembly.instructions_by_address[instruction.address] = instruction;
	}
}

Disassembly Disassemble(const std::vector<uint8_t> &memory, uint16_t start_address, std::vector<uint16_t> entry_points)
{
	PartialDisassembly partialDisassembly;
	partialDisassembly.remaining_entry_points = entry_points;

	while(!partialDisassembly.remaining_entry_points.empty())
	{
		// pull the next entry point from the back of the vector
		uint16_t next_entry_point = partialDisassembly.remaining_entry_points.back();
		partialDisassembly.remaining_entry_points.pop_back();

		// if that address has already bene visited, forget about it
		if(partialDisassembly.disassembly.instructions_by_address.find(next_entry_point) != partialDisassembly.disassembly.instructions_by_address.end()) continue;

		// otherwise perform a diassembly run
		AddToDisassembly(partialDisassembly, memory, start_address, next_entry_point);
	}

	return std::move(partialDisassembly.disassembly);
}
