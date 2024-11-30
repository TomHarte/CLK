//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Cartridge.hpp"
#include "Tape.hpp"
#include "Target.hpp"

#include "../Disassembler/Z80.hpp"
#include "../Disassembler/AddressMapper.hpp"

#include <algorithm>

static std::unique_ptr<Analyser::Static::Target> CartridgeTarget(
	const Storage::Cartridge::Cartridge::Segment &segment,
	uint16_t start_address,
	Analyser::Static::MSX::Cartridge::Type type,
	float confidence) {

	// Size down to a multiple of 8kb in size and apply the start address.
	std::vector<Storage::Cartridge::Cartridge::Segment> output_segments;
	if(segment.data.size() & 0x1fff) {
		std::vector<uint8_t> truncated_data;
		const auto truncated_size =
			std::vector<uint8_t>::difference_type(segment.data.size()) & ~0x1fff;
		truncated_data.insert(truncated_data.begin(), segment.data.begin(), segment.data.begin() + truncated_size);
		output_segments.emplace_back(start_address, truncated_data);
	} else {
		output_segments.emplace_back(start_address, segment.data);
	}

	auto target = std::make_unique<Analyser::Static::MSX::Target>();
	target->confidence = confidence;

	// Observation: all ROMs of 48kb or less are from the MSX 1 era.
	if(segment.data.size() < 48*1024) {
		target->model = Analyser::Static::MSX::Target::Model::MSX1;
	}

	if(type == Analyser::Static::MSX::Cartridge::Type::None) {
		target->media.cartridges.emplace_back(new Storage::Cartridge::Cartridge(output_segments));
	} else {
		target->media.cartridges.emplace_back(new Analyser::Static::MSX::Cartridge(output_segments, type));
	}

	return target;
}

/*
	Expected standard cartridge format:

		DEFB "AB" ; expansion ROM header
		DEFW initcode ; start of the init code, 0 if no initcode
		DEFW callstat; pointer to CALL statement handler, 0 if no such handler
		DEFW device; pointer to expansion device handler, 0 if no such handler
		DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
		DEFS 6,0 ; room reserved for future extensions

	MSX cartridges often include banking hardware; those games were marketed as MegaROMs. The file
	format that the MSX community has decided upon doesn't retain the type of hardware included, so
	this analyser has to guess.

	(additional audio hardware is also sometimes included, but it's implied by the banking hardware)
*/
static Analyser::Static::TargetList CartridgeTargetsFrom(
	const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	// No cartridges implies no targets.
	if(cartridges.empty()) {
		return {};
	}

	Analyser::Static::TargetList targets;
	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// Only one mapped item is allowed.
		if(segments.size() != 1) continue;

		// Which must be no more than 63 bytes larger than a multiple of 8 kb in size.
		const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
		const size_t data_size = segment.data.size();
		if(data_size < 0x2000 || (data_size & 0x1fff) > 64) continue;

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

		const uint16_t init_address = uint16_t(segment.data[2] | (segment.data[3] << 8));
		// TODO: check for a rational init address?

		// If this ROM is less than 48kb in size then it's an ordinary ROM. Just emplace it and move on.
		// Bonus observation: all such ROMs are from the MSX 1 era.
		if(data_size <= 0xc000) {
			targets.emplace_back(CartridgeTarget(segment, start_address, Analyser::Static::MSX::Cartridge::Type::None, 1.0));
			continue;
		}

		// If this ROM is greater than 48kb in size then some sort of MegaROM scheme must
		// be at play; disassemble to try to figure it out.
		std::vector<uint8_t> first_8k;
		first_8k.insert(first_8k.begin(), segment.data.begin(), segment.data.begin() + 8192);
		const Analyser::Static::Z80::Disassembly disassembly =
			Analyser::Static::Z80::Disassemble(
				first_8k,
				Analyser::Static::Disassembler::OffsetMapper(start_address),
				{ init_address },
				Analyser::Static::Z80::Approach::Exhaustive
			);

		using Instruction = Analyser::Static::Z80::Instruction;
		const std::map<uint16_t, Instruction> &instructions = disassembly.instructions_by_address;

		// Look for LD (nnnn), A instructions, and collate those addresses.
		std::map<uint16_t, int> address_counts;
		for(const auto &instruction_pair : instructions) {
			if(	instruction_pair.second.operation == Instruction::Operation::LD &&
				instruction_pair.second.destination == Instruction::Location::Operand_Indirect &&
				instruction_pair.second.source == Instruction::Location::A) {
				address_counts[uint16_t(instruction_pair.second.operand)]++;
			}
		}

		// Weight confidences by number of observed hits; if any is above 60% confidence, just use it.
		const auto ascii_8kb_total =
			address_counts[0x6000] + address_counts[0x6800] + address_counts[0x7000] + address_counts[0x7800];
		const auto ascii_16kb_total = address_counts[0x6000] + address_counts[0x7000] + address_counts[0x77ff];
		const auto konami_total = address_counts[0x6000] + address_counts[0x8000] + address_counts[0xa000];
		const auto konami_with_scc_total =
			address_counts[0x5000] + address_counts[0x7000] + address_counts[0x9000] + address_counts[0xb000];

		const auto total_hits = ascii_8kb_total + ascii_16kb_total + konami_total + konami_with_scc_total;

		const bool is_ascii_8kb = (ascii_8kb_total * 5) / (total_hits * 3);
		const bool is_ascii_16kb = (ascii_16kb_total * 5) / (total_hits * 3);
		const bool is_konami = (konami_total * 5) / (total_hits * 3);
		const bool is_konami_with_scc = (konami_with_scc_total * 5) / (total_hits * 3);

		if(!is_ascii_16kb && !is_konami && !is_konami_with_scc) {
			targets.push_back(CartridgeTarget(
				segment,
				start_address,
				Analyser::Static::MSX::Cartridge::ASCII8kb,
				float(ascii_8kb_total) / float(total_hits)));
		}
		if(!is_ascii_8kb && !is_konami && !is_konami_with_scc) {
			targets.push_back(CartridgeTarget(
				segment,
				start_address,
				Analyser::Static::MSX::Cartridge::ASCII16kb,
				float(ascii_16kb_total) / float(total_hits)));
		}
		if(!is_ascii_8kb && !is_ascii_16kb && !is_konami_with_scc) {
			targets.push_back(CartridgeTarget(
				segment,
				start_address,
				Analyser::Static::MSX::Cartridge::Konami,
				float(konami_total) / float(total_hits)));
		}
		if(!is_ascii_8kb && !is_ascii_16kb && !is_konami) {
			targets.push_back(CartridgeTarget(
				segment,
				start_address,
				Analyser::Static::MSX::Cartridge::KonamiWithSCC,
				float(konami_with_scc_total) / float(total_hits)));
		}
	}

	return targets;
}

Analyser::Static::TargetList Analyser::Static::MSX::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType
) {
	TargetList destination;

	// Append targets for any cartridges that look correct.
	auto cartridge_targets = CartridgeTargetsFrom(media.cartridges);
	std::move(cartridge_targets.begin(), cartridge_targets.end(), std::back_inserter(destination));

	// Consider building a target for disks and/or tapes.
	auto target = std::make_unique<Target>();

	// Check tapes for loadable files.
	for(auto &tape : media.tapes) {
		const std::vector<File> files_on_tape = GetFiles(tape);
		if(!files_on_tape.empty()) {
			switch(files_on_tape.front().type) {
				case File::Type::ASCII:				target->loading_command = "RUN\"CAS:\r";		break;
				case File::Type::TokenisedBASIC:	target->loading_command = "CLOAD\rRUN\r";		break;
				case File::Type::Binary:			target->loading_command = "BLOAD\"CAS:\",R\r";	break;
				default: break;
			}
			target->media.tapes.push_back(tape);
		}
	}

	// Region selection: for now, this as simple as:
	// "If a tape is involved, be European. Otherwise be American (i.e. English, but 60Hz)".
	target->region = target->media.tapes.empty() ? Target::Region::USA : Target::Region::Europe;

	// Blindly accept disks for now.
	// TODO: how to spot an MSX disk?
	target->media.disks = media.disks;
	target->has_disk_drive = !media.disks.empty();

	if(!target->media.empty()) {
		target->confidence = 0.5;
		destination.push_back(std::move(target));
	}

	return destination;
}
