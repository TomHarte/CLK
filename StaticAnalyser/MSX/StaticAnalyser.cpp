//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Tape.hpp"
#include "../Disassembler/Z80.hpp"
#include "../Disassembler/AddressMapper.hpp"

#include <algorithm>

/*
	Expected standard cartridge format:

		DEFB "AB" ; expansion ROM header
		DEFW initcode ; start of the init code, 0 if no initcode
		DEFW callstat; pointer to CALL statement handler, 0 if no such handler
		DEFW device; pointer to expansion device handler, 0 if no such handler
		DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
		DEFS 6,0 ; room reserved for future extensions
*/
static std::list<std::shared_ptr<Storage::Cartridge::Cartridge>>
		MSXCartridgesFrom(const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges, StaticAnalyser::Target &target) {
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> msx_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// Only one mapped item is allowed.
		if(segments.size() != 1) continue;

		// Which must be a multiple of 16 kb in size.
		Storage::Cartridge::Cartridge::Segment segment = segments.front();
		const size_t data_size = segment.data.size();
		if(data_size < 0x2000 || data_size & 0x3fff) continue;

		// Check for a ROM header at address 0; if it's not found then try 0x4000
		// and adjust the start address;
		uint16_t start_address = 0;
		bool found_start = false;
		if(segment.data[0] == 0x41 && segment.data[1] == 0x42) {
			start_address = 0x4000;
			found_start = true;
		} else if(segment.data.size() >= 0x8000 && segment.data[0x4000] == 0x41 && segment.data[0x4001] == 0x42) {
			start_address = 0;
			found_start = true;
		}

		// Reject cartridge if the ROM header wasn't found.
		if(!found_start) continue;

		uint16_t init_address = static_cast<uint16_t>(segment.data[2] | (segment.data[3] << 8));
		// TODO: check for a rational init address?

		// If this ROM is greater than 48kb in size then some sort of MegaROM scheme must
		// be at play; disassemble to try to figure it out.
		target.msx.paging_model = StaticAnalyser::MSXCartridgeType::None;
		if(data_size > 0xc000) {
			std::vector<uint8_t> first_16k;
			first_16k.insert(first_16k.begin(), segment.data.begin(), segment.data.begin() + 8192);
			StaticAnalyser::Z80::Disassembly disassembly =
				StaticAnalyser::Z80::Disassemble(
					first_16k,
					StaticAnalyser::Disassembler::OffsetMapper(start_address),
					{ init_address }
				);

			// Look for a indirect store followed by an unconditional JP or CALL into another
			// segment, that's a fairly explicit sign where found.
			using Instruction = StaticAnalyser::Z80::Instruction;
			std::map<uint16_t, Instruction> &instructions = disassembly.instructions_by_address;
			bool is_ascii = false;
			auto iterator = instructions.begin();
			while(iterator != instructions.end()) {
				auto next_iterator = iterator;
				next_iterator++;
				if(next_iterator == instructions.end()) break;

				if(	iterator->second.operation == Instruction::Operation::LD &&
					iterator->second.destination == Instruction::Location::Operand_Indirect &&
					(
						iterator->second.operand == 0x5000 ||
						iterator->second.operand == 0x6000 ||
						iterator->second.operand == 0x6800 ||
						iterator->second.operand == 0x7000 ||
						iterator->second.operand == 0x77ff ||
						iterator->second.operand == 0x7800 ||
						iterator->second.operand == 0x8000 ||
						iterator->second.operand == 0x9000 ||
						iterator->second.operand == 0xa000
					) &&
					(
						next_iterator->second.operation == Instruction::Operation::CALL ||
						next_iterator->second.operation == Instruction::Operation::JP
					) &&
					((next_iterator->second.operand >> 13) != (0x4000 >> 13))
				) {
					const uint16_t address = static_cast<uint16_t>(next_iterator->second.operand);
					switch(iterator->second.operand) {
						case 0x6000:
							if(address >= 0x6000 && address < 0x8000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::KonamiWithSCC;
							}
						break;
						case 0x6800:
							if(address >= 0x6000 && address < 0x6800) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::ASCII8kb;
							}
						break;
						case 0x7000:
							if(address >= 0x6000 && address < 0x8000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::KonamiWithSCC;
							}
							if(address >= 0x7000 && address < 0x7800) {
								is_ascii = true;
							}
						break;
						case 0x77ff:
							if(address >= 0x7000 && address < 0x7800) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::ASCII16kb;
							}
						break;
						case 0x7800:
							if(address >= 0xa000 && address < 0xc000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::ASCII8kb;
							}
						break;
						case 0x8000:
							if(address >= 0x8000 && address < 0xa000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::KonamiWithSCC;
							}
						break;
						case 0x9000:
							if(address >= 0x8000 && address < 0xa000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::KonamiWithSCC;
							}
						break;
						case 0xa000:
							if(address >= 0xa000 && address < 0xc000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::Konami;
							}
						break;
						case 0xb000:
							if(address >= 0xa000 && address < 0xc000) {
								target.msx.paging_model = StaticAnalyser::MSXCartridgeType::KonamiWithSCC;
							}
						break;
					}
				}

				iterator = next_iterator;
			}

			if(target.msx.paging_model == StaticAnalyser::MSXCartridgeType::None) {
				// Look for LD (nnnn), A instructions, and collate those addresses.
				std::map<uint16_t, int> address_counts;
				for(const auto &instruction_pair : instructions) {
					if(	instruction_pair.second.operation == Instruction::Operation::LD &&
						instruction_pair.second.destination == Instruction::Location::Operand_Indirect &&
						instruction_pair.second.source == Instruction::Location::A) {
						address_counts[static_cast<uint16_t>(instruction_pair.second.operand)]++;
					}
				}

				// Sort possible cartridge types.
				using Possibility = std::pair<StaticAnalyser::MSXCartridgeType, int>;
				std::vector<Possibility> possibilities;
				// Add to list in order of declining probability, so that stable_sort below prefers
				// the more likely option in a tie.
				possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::ASCII8kb, address_counts[0x6000] + address_counts[0x6800] + address_counts[0x7000] + address_counts[0x7800]));
				possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::ASCII16kb, address_counts[0x6000] + address_counts[0x7000] + address_counts[0x77ff]));
				if(!is_ascii) possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::Konami, address_counts[0x6000] + address_counts[0x8000] + address_counts[0xa000]));
				if(!is_ascii) possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::KonamiWithSCC, address_counts[0x5000] + address_counts[0x7000] + address_counts[0x9000] + address_counts[0xb000]));
				std::stable_sort(possibilities.begin(), possibilities.end(), [](const Possibility &a, const Possibility &b) {
					return a.second > b.second;
				});

				target.msx.paging_model = possibilities[0].first;
			}
		}

		// Apply the standard MSX start address.
		msx_cartridges.emplace_back(new Storage::Cartridge::Cartridge({
			Storage::Cartridge::Cartridge::Segment(start_address, segment.data)
		}));
	}

	return msx_cartridges;
}

void StaticAnalyser::MSX::AddTargets(const Media &media, std::list<Target> &destination) {
	Target target;

	// Obtain only those cartridges which it looks like an MSX would understand.
	target.media.cartridges = MSXCartridgesFrom(media.cartridges, target);

	// Check tapes for loadable files.
	for(const auto &tape : media.tapes) {
		std::vector<File> files_on_tape = GetFiles(tape);
		if(!files_on_tape.empty()) {
			switch(files_on_tape.front().type) {
				case File::Type::ASCII:				target.loading_command = "RUN\"CAS:\r";			break;
				case File::Type::TokenisedBASIC:	target.loading_command = "CLOAD\rRUN\r";		break;
				case File::Type::Binary:			target.loading_command = "BLOAD\"CAS:\",R\r";	break;
				default: break;
			}
			target.media.tapes.push_back(tape);
		}
	}

	if(!target.media.empty()) {
		target.machine = Target::MSX;
		target.probability = 1.0;
		destination.push_back(target);
	}
}
