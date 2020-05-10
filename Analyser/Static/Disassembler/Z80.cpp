//
//  Z80.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Z80.hpp"

#include "Kernel.hpp"

using namespace Analyser::Static::Z80;
namespace  {

using PartialDisassembly = Analyser::Static::Disassembly::PartialDisassembly<Disassembly, uint16_t>;

class Accessor {
	public:
		Accessor(const std::vector<uint8_t> &memory, const std::function<std::size_t(uint16_t)> &address_mapper, uint16_t address) :
			memory_(memory), address_mapper_(address_mapper), address_(address) {}

		uint8_t byte() {
			std::size_t mapped_address = address_mapper_(address_);
			address_++;
			if(mapped_address >= memory_.size()) {
				overrun_ = true;
				return 0xff;
			}
			return memory_[mapped_address];
		}

		uint16_t word() {
			uint8_t low = byte();
			uint8_t high = byte();
			return uint16_t(low | (high << 8));
		}

		bool overrun() {
			return overrun_;
		}

		bool at_end() {
			std::size_t mapped_address = address_mapper_(address_);
			return mapped_address >= memory_.size();
		}

		uint16_t address() {
			return address_;
		}

	private:
		const std::vector<uint8_t> &memory_;
		const std::function<std::size_t(uint16_t)> &address_mapper_;
		uint16_t address_;
		bool overrun_ = false;
};

#define x(v) (v >> 6)
#define y(v) ((v >> 3) & 7)
#define q(v) ((v >> 3) & 1)
#define p(v) ((v >> 4) & 3)
#define z(v) (v & 7)

Instruction::Condition condition_table[] = {
	Instruction::Condition::NZ,		Instruction::Condition::Z,
	Instruction::Condition::NC,		Instruction::Condition::C,
	Instruction::Condition::PO,		Instruction::Condition::PE,
	Instruction::Condition::P,		Instruction::Condition::M
};

Instruction::Location register_pair_table[] = {
	Instruction::Location::BC,
	Instruction::Location::DE,
	Instruction::Location::HL,
	Instruction::Location::SP
};

Instruction::Location register_pair_table2[] = {
	Instruction::Location::BC,
	Instruction::Location::DE,
	Instruction::Location::HL,
	Instruction::Location::AF
};

Instruction::Location RegisterTableEntry(int offset, Accessor &accessor, Instruction &instruction, bool needs_indirect_offset) {
	Instruction::Location register_table[] = {
		Instruction::Location::B,	Instruction::Location::C,
		Instruction::Location::D,	Instruction::Location::E,
		Instruction::Location::H,	Instruction::Location::L,
		Instruction::Location::HL_Indirect,
		Instruction::Location::A
	};

	Instruction::Location location = register_table[offset];
	if(location == Instruction::Location::HL_Indirect && needs_indirect_offset) {
		instruction.offset = accessor.byte() - 128;
	}

	return location;
}

Instruction::Operation alu_table[] = {
	Instruction::Operation::ADD,
	Instruction::Operation::ADC,
	Instruction::Operation::SUB,
	Instruction::Operation::SBC,
	Instruction::Operation::AND,
	Instruction::Operation::XOR,
	Instruction::Operation::OR,
	Instruction::Operation::CP
};

Instruction::Operation rotation_table[] = {
	Instruction::Operation::RLC,
	Instruction::Operation::RRC,
	Instruction::Operation::RL,
	Instruction::Operation::RR,
	Instruction::Operation::SLA,
	Instruction::Operation::SRA,
	Instruction::Operation::SLL,
	Instruction::Operation::SRL
};

Instruction::Operation block_table[][4] = {
	{Instruction::Operation::LDI, Instruction::Operation::CPI, Instruction::Operation::INI, Instruction::Operation::OUTI},
	{Instruction::Operation::LDD, Instruction::Operation::CPD, Instruction::Operation::IND, Instruction::Operation::OUTD},
	{Instruction::Operation::LDIR, Instruction::Operation::CPIR, Instruction::Operation::INIR, Instruction::Operation::OTIR},
	{Instruction::Operation::LDDR, Instruction::Operation::CPDR, Instruction::Operation::INDR, Instruction::Operation::OTDR},
};

void DisassembleCBPage(Accessor &accessor, Instruction &instruction, bool needs_indirect_offset) {
	const uint8_t operation = accessor.byte();

	if(!x(operation)) {
		instruction.operation = rotation_table[y(operation)];
		instruction.source = instruction.destination = RegisterTableEntry(z(operation), accessor, instruction, needs_indirect_offset);
	} else {
		instruction.destination = RegisterTableEntry(z(operation), accessor, instruction, needs_indirect_offset);
		instruction.source = Instruction::Location::Operand;
		instruction.operand = y(operation);

		switch(x(operation)) {
			case 1:	instruction.operation = Instruction::Operation::BIT;	break;
			case 2:	instruction.operation = Instruction::Operation::RES;	break;
			case 3:	instruction.operation = Instruction::Operation::SET;	break;
		}
	}
}

void DisassembleEDPage(Accessor &accessor, Instruction &instruction, bool needs_indirect_offset) {
	const uint8_t operation = accessor.byte();

	switch(x(operation)) {
		default:
			instruction.operation = Instruction::Operation::Invalid;
		break;
		case 2:
			if(z(operation) < 4 && y(operation) >= 4) {
				instruction.operation = block_table[y(operation)-4][z(operation)];
			} else {
				instruction.operation = Instruction::Operation::Invalid;
			}
		break;
		case 3:
			switch(z(operation)) {
				case 0:
					instruction.operation = Instruction::Operation::IN;
					instruction.source = Instruction::Location::BC_Indirect;
					if(y(operation) == 6) {
						instruction.destination = Instruction::Location::None;
					} else {
						instruction.destination = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
					}
				break;
				case 1:
					instruction.operation = Instruction::Operation::OUT;
					instruction.destination = Instruction::Location::BC_Indirect;
					if(y(operation) == 6) {
						instruction.source = Instruction::Location::None;
					} else {
						instruction.source = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
					}
				break;
				case 2:
					instruction.operation = (y(operation)&1) ? Instruction::Operation::ADC : Instruction::Operation::SBC;
					instruction.destination = Instruction::Location::HL;
					instruction.source = register_pair_table[y(operation) >> 1];
				break;
				case 3:
					instruction.operation = Instruction::Operation::LD;
					if(q(operation)) {
						instruction.destination = RegisterTableEntry(p(operation), accessor, instruction, needs_indirect_offset);
						instruction.source = Instruction::Location::Operand_Indirect;
					} else {
						instruction.destination = Instruction::Location::Operand_Indirect;
						instruction.source = RegisterTableEntry(p(operation), accessor, instruction, needs_indirect_offset);
					}
					instruction.operand = accessor.word();
				break;
				case 4:
					instruction.operation = Instruction::Operation::NEG;
				break;
				case 5:
					instruction.operation = (y(operation) == 1) ? Instruction::Operation::RETI : Instruction::Operation::RETN;
				break;
				case 6:
					instruction.operation = Instruction::Operation::IM;
					instruction.source = Instruction::Location::Operand;
					switch(y(operation)&3) {
						case 0:	instruction.operand = 0;	break;
						case 1:	instruction.operand = 0;	break;
						case 2:	instruction.operand = 1;	break;
						case 3:	instruction.operand = 2;	break;
					}
				break;
				case 7:
					switch(y(operation)) {
						case 0:
							instruction.operation = Instruction::Operation::LD;
							instruction.destination = Instruction::Location::I;
							instruction.source = Instruction::Location::A;
						break;
						case 1:
							instruction.operation = Instruction::Operation::LD;
							instruction.destination = Instruction::Location::R;
							instruction.source = Instruction::Location::A;
						break;
						case 2:
							instruction.operation = Instruction::Operation::LD;
							instruction.destination = Instruction::Location::A;
							instruction.source = Instruction::Location::I;
						break;
						case 3:
							instruction.operation = Instruction::Operation::LD;
							instruction.destination = Instruction::Location::A;
							instruction.source = Instruction::Location::R;
						break;
						case 4:		instruction.operation = Instruction::Operation::RRD;	break;
						case 5:		instruction.operation = Instruction::Operation::RLD;	break;
						default:	instruction.operation = Instruction::Operation::NOP;	break;
					}
				break;
			}
		break;
	}
}

void DisassembleMainPage(Accessor &accessor, Instruction &instruction) {
	bool needs_indirect_offset = false;
	enum HLSubstitution {
		None, IX, IY
	} hl_substitution = None;

	while(true) {
		uint8_t operation = accessor.byte();

		switch(x(operation)) {
			case 0:
				switch(z(operation)) {
					case 0:
						switch(y(operation)) {
							case 0: instruction.operation = Instruction::Operation::NOP;		break;
							case 1: instruction.operation = Instruction::Operation::EXAFAFd;	break;
							case 2:
								instruction.operation = Instruction::Operation::DJNZ;
								instruction.operand = accessor.byte() - 128;
							break;
							default:
								instruction.operation = Instruction::Operation::JR;
								instruction.operand = accessor.byte() - 128;
								if(y(operation) >= 4) instruction.condition = condition_table[y(operation) - 4];
							break;
						}
					break;
					case 1:
						if(y(operation)&1) {
							instruction.operation = Instruction::Operation::ADD;
							instruction.destination = Instruction::Location::HL;
							instruction.source = register_pair_table[y(operation) >> 1];
						} else {
							instruction.operation = Instruction::Operation::LD;
							instruction.destination = register_pair_table[y(operation) >> 1];
							instruction.source = Instruction::Location::Operand;
							instruction.operand = accessor.word();
						}
					break;
					case 2:
						switch(y(operation)) {
							case 0:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::BC_Indirect;
								instruction.source = Instruction::Location::A;
							break;
							case 1:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::A;
								instruction.source = Instruction::Location::BC_Indirect;
							break;
							case 2:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::DE_Indirect;
								instruction.source = Instruction::Location::A;
							break;
							case 3:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::A;
								instruction.source = Instruction::Location::DE_Indirect;
							break;
							case 4:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::Operand_Indirect;
								instruction.source = Instruction::Location::HL;
							break;
							case 5:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::HL;
								instruction.source = Instruction::Location::Operand_Indirect;
							break;
							case 6:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::Operand_Indirect;
								instruction.source = Instruction::Location::A;
							break;
							case 7:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::A;
								instruction.source = Instruction::Location::Operand_Indirect;
							break;
						}

						if(y(operation) > 3) {
							instruction.operand = accessor.word();
						}
					break;
					case 3:
						if(y(operation)&1) {
							instruction.operation = Instruction::Operation::DEC;
						} else {
							instruction.operation = Instruction::Operation::INC;
						}
						instruction.source = instruction.destination = register_pair_table[y(operation) >> 1];
					break;
					case 4:
						instruction.operation = Instruction::Operation::INC;
						instruction.source = instruction.destination = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
					break;
					case 5:
						instruction.operation = Instruction::Operation::DEC;
						instruction.source = instruction.destination = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
					break;
					case 6:
						instruction.operation = Instruction::Operation::LD;
						instruction.destination = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
						instruction.source = Instruction::Location::Operand;
						instruction.operand = accessor.byte();
					break;
					case 7:
						switch(y(operation)) {
							case 0:	instruction.operation = Instruction::Operation::RLCA;	break;
							case 1:	instruction.operation = Instruction::Operation::RRCA;	break;
							case 2:	instruction.operation = Instruction::Operation::RLA;	break;
							case 3:	instruction.operation = Instruction::Operation::RRA;	break;
							case 4:	instruction.operation = Instruction::Operation::DAA;	break;
							case 5:	instruction.operation = Instruction::Operation::CPL;	break;
							case 6:	instruction.operation = Instruction::Operation::SCF;	break;
							case 7:	instruction.operation = Instruction::Operation::CCF;	break;
						}
					break;
				}
			break;
			case 1:
				if(y(operation) == 6 && z(operation) == 6) {
					instruction.operation = Instruction::Operation::HALT;
				} else {
					instruction.operation = Instruction::Operation::LD;
					instruction.source = RegisterTableEntry(z(operation), accessor, instruction, needs_indirect_offset);
					instruction.destination = RegisterTableEntry(y(operation), accessor, instruction, needs_indirect_offset);
				}
			break;
			case 2:
				instruction.operation = alu_table[y(operation)];
				instruction.source = RegisterTableEntry(z(operation), accessor, instruction, needs_indirect_offset);
				instruction.destination = Instruction::Location::A;
			break;
			case 3:
				switch(z(operation)) {
					case 0:
						instruction.operation = Instruction::Operation::RET;
						instruction.condition = condition_table[y(operation)];
					break;
					case 1:
						switch(y(operation)) {
							default:
								instruction.operation = Instruction::Operation::POP;
								instruction.source = register_pair_table2[y(operation) >> 1];
							break;
							case 1:
								instruction.operation = Instruction::Operation::RET;
							break;
							case 3:
								instruction.operation = Instruction::Operation::EXX;
							break;
							case 5:
								instruction.operation = Instruction::Operation::JP;
								instruction.source = Instruction::Location::HL;
							break;
							case 7:
								instruction.operation = Instruction::Operation::LD;
								instruction.destination = Instruction::Location::SP;
								instruction.source = Instruction::Location::HL;
							break;
						}
					break;
					case 2:
						instruction.operation = Instruction::Operation::JP;
						instruction.condition = condition_table[y(operation)];
						instruction.operand = accessor.word();
					break;
					case 3:
						switch(y(operation)) {
							case 0:
								instruction.operation = Instruction::Operation::JP;
								instruction.source = Instruction::Location::Operand;
								instruction.operand = accessor.word();
							break;
							case 1:
								DisassembleCBPage(accessor, instruction, needs_indirect_offset);
							break;
							case 2:
								instruction.operation = Instruction::Operation::OUT;
								instruction.source = Instruction::Location::A;
								instruction.destination = Instruction::Location::Operand_Indirect;
								instruction.operand = accessor.byte();
							break;
							case 3:
								instruction.operation = Instruction::Operation::IN;
								instruction.destination = Instruction::Location::A;
								instruction.source = Instruction::Location::Operand_Indirect;
								instruction.operand = accessor.byte();
							break;
							case 4:
								instruction.operation = Instruction::Operation::EX;
								instruction.destination = Instruction::Location::SP_Indirect;
								instruction.source = Instruction::Location::HL;
							break;
							case 5:
								instruction.operation = Instruction::Operation::EX;
								instruction.destination = Instruction::Location::DE;
								instruction.source = Instruction::Location::HL;
							break;
							case 6:
								instruction.operation = Instruction::Operation::DI;
							break;
							case 7:
								instruction.operation = Instruction::Operation::EI;
							break;
						}
					break;
					case 4:
						instruction.operation = Instruction::Operation::CALL;
						instruction.source = Instruction::Location::Operand_Indirect;
						instruction.operand = accessor.word();
						instruction.condition = condition_table[y(operation)];
					break;
					case 5:
						switch(y(operation)) {
							default:
								instruction.operation = Instruction::Operation::PUSH;
								instruction.source = register_pair_table2[y(operation) >> 1];
							break;
							case 1:
								instruction.operation = Instruction::Operation::CALL;
								instruction.source = Instruction::Location::Operand;
								instruction.operand = accessor.word();
							break;
							case 3:
								needs_indirect_offset = true;
								hl_substitution = IX;
							continue;	// i.e. repeat loop.
							case 5:
								DisassembleEDPage(accessor, instruction, needs_indirect_offset);
							break;
							case 7:
								needs_indirect_offset = true;
								hl_substitution = IY;
							continue;	// i.e. repeat loop.
						}
					break;
					case 6:
						instruction.operation = alu_table[y(operation)];
						instruction.source = Instruction::Location::Operand;
						instruction.destination = Instruction::Location::A;
						instruction.operand = accessor.byte();
					break;
					case 7:
						instruction.operation = Instruction::Operation::RST;
						instruction.source = Instruction::Location::Operand;
						instruction.operand = y(operation) << 3;
					break;
				}
			break;
		}

		// This while(true) isn't an infinite loop for everything except those paths that opt in
		// via continue.
		break;
	}

	// Perform IX/IY substitution for HL, if applicable.
	if(hl_substitution != None) {
		// EX DE, HL is not affected.
		if(instruction.operation == Instruction::Operation::EX) return;

		// If an (HL) is involved, switch it for IX+d or IY+d.
		if(	instruction.source == Instruction::Location::HL_Indirect ||
			instruction.destination == Instruction::Location::HL_Indirect) {

			if(instruction.source == Instruction::Location::HL_Indirect) {
				instruction.source = (hl_substitution == IX) ? Instruction::Location::IX_Indirect_Offset : Instruction::Location::IY_Indirect_Offset;
			}
			if(instruction.destination == Instruction::Location::HL_Indirect) {
				instruction.destination = (hl_substitution == IX) ? Instruction::Location::IX_Indirect_Offset : Instruction::Location::IY_Indirect_Offset;
			}
			return;
		}

		// Otherwise, switch either of H or L for I[X/Y]h and I[X/Y]l.
		if(instruction.source == Instruction::Location::H) {
			instruction.source = (hl_substitution == IX) ? Instruction::Location::IXh : Instruction::Location::IYh;
		}
		if(instruction.source == Instruction::Location::L) {
			instruction.source = (hl_substitution == IX) ? Instruction::Location::IXl : Instruction::Location::IYl;
		}
		if(instruction.destination == Instruction::Location::H) {
			instruction.destination = (hl_substitution == IX) ? Instruction::Location::IXh : Instruction::Location::IYh;
		}
		if(instruction.destination == Instruction::Location::L) {
			instruction.destination = (hl_substitution == IX) ? Instruction::Location::IXl : Instruction::Location::IYl;
		}
	}
}

struct Z80Disassembler {
	static void AddToDisassembly(PartialDisassembly &disassembly, const std::vector<uint8_t> &memory, const std::function<std::size_t(uint16_t)> &address_mapper, uint16_t entry_point) {
		disassembly.disassembly.internal_calls.insert(entry_point);
		Accessor accessor(memory, address_mapper, entry_point);

		while(!accessor.at_end()) {
			Instruction instruction;
			instruction.address = accessor.address();

			DisassembleMainPage(accessor, instruction);

			// If any memory access was invalid, end disassembly.
			if(accessor.overrun()) return;

			// Store the instruction away.
			disassembly.disassembly.instructions_by_address[instruction.address] = instruction;

			// Update access tables.
			int access_type =
				((instruction.source == Instruction::Location::Operand_Indirect) ? 1 : 0) |
				((instruction.destination == Instruction::Location::Operand_Indirect) ? 2 : 0);
			uint16_t address = uint16_t(instruction.operand);
			bool is_internal = address_mapper(address) < memory.size();
			switch(access_type) {
				default: break;
				case 1:
					if(is_internal) {
						disassembly.disassembly.internal_loads.insert(address);
					} else {
						disassembly.disassembly.external_loads.insert(address);
					}
				break;
				case 2:
					if(is_internal) {
						disassembly.disassembly.internal_stores.insert(address);
					} else {
						disassembly.disassembly.external_stores.insert(address);
					}
				break;
				case 3:
					if(is_internal) {
						disassembly.disassembly.internal_modifies.insert(address);
					} else {
						disassembly.disassembly.internal_modifies.insert(address);
					}
				break;
			}

			// Add any (potentially) newly discovered entry point.
			if(	instruction.operation == Instruction::Operation::JP ||
				instruction.operation == Instruction::Operation::JR ||
				instruction.operation == Instruction::Operation::CALL ||
				instruction.operation == Instruction::Operation::RST) {
				disassembly.remaining_entry_points.push_back(uint16_t(instruction.operand));
			}

			// This is it if: an unconditional RET, RETI, RETN, JP or JR is found.
			if(instruction.condition != Instruction::Condition::None)	continue;

			if(instruction.operation == Instruction::Operation::RET)	return;
			if(instruction.operation == Instruction::Operation::RETI)	return;
			if(instruction.operation == Instruction::Operation::RETN)	return;
			if(instruction.operation == Instruction::Operation::JP)		return;
			if(instruction.operation == Instruction::Operation::JR)		return;
		}
	}
};

}	// end of anonymous namespace

Disassembly Analyser::Static::Z80::Disassemble(
	const std::vector<uint8_t> &memory,
	const std::function<std::size_t(uint16_t)> &address_mapper,
	std::vector<uint16_t> entry_points) {
	return Analyser::Static::Disassembly::Disassemble<Disassembly, uint16_t, Z80Disassembler>(memory, address_mapper, entry_points);
}
