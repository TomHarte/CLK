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
		if(data_size > 0xc000) {
			std::vector<uint8_t> first_segment;
			first_segment.insert(first_segment.begin(), segment.data.begin(), segment.data.begin() + 32768);
			StaticAnalyser::Z80::Disassembly disassembly =
				StaticAnalyser::Z80::Disassemble(
					first_segment,
					StaticAnalyser::Disassembler::OffsetMapper(start_address),
					{ init_address }
				);

			// Look for LD (nnnn), A instructions, and collate those addresses.
			using Instruction = StaticAnalyser::Z80::Instruction;
			std::map<uint16_t, int> address_counts;
			for(const auto &instruction_pair : disassembly.instructions_by_address) {
				if(	instruction_pair.second.operation == Instruction::Operation::LD &&
					instruction_pair.second.destination == Instruction::Location::Operand_Indirect &&
					instruction_pair.second.source == Instruction::Location::A) {
					address_counts[static_cast<uint16_t>(instruction_pair.second.operand)]++;
				}
			}

			// Sort possible cartridge types.
			using Possibility = std::pair<StaticAnalyser::MSXCartridgeType, int>;
			std::vector<Possibility> possibilities;
			possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::Konami, address_counts[0x6000] + address_counts[0x8000] + address_counts[0xa000]));
			possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::KonamiWithSCC, address_counts[0x5000] + address_counts[0x7000] + address_counts[0x9000] + address_counts[0xb000]));
			possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::ASCII8kb, address_counts[0x6000] + address_counts[0x6800] + address_counts[0x7000] + address_counts[0x7800]));
			possibilities.push_back(std::make_pair(StaticAnalyser::MSXCartridgeType::ASCII16kb, address_counts[0x6000] + address_counts[0x7000] + address_counts[0x77ff]));
			std::sort(possibilities.begin(), possibilities.end(), [](const Possibility &a, const Possibility &b) {
				return a.second > b.second;
			});

			target.msx.paging_model = possibilities[0].first;
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
