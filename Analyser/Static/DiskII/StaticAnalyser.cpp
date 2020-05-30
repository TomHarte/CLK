//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../AppleII/Target.hpp"
#include "../Oric/Target.hpp"
#include "../Disassembler/6502.hpp"
#include "../Disassembler/AddressMapper.hpp"

#include "../../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../../Storage/Disk/Encodings/AppleGCR/SegmentParser.hpp"

namespace {

Analyser::Static::Target *AppleTarget(const Storage::Encodings::AppleGCR::Sector *sector_zero) {
	using Target = Analyser::Static::AppleII::Target;
	auto *const target = new Target;

	if(sector_zero && sector_zero->encoding == Storage::Encodings::AppleGCR::Sector::Encoding::FiveAndThree) {
		target->disk_controller = Target::DiskController::ThirteenSector;
	} else {
		target->disk_controller = Target::DiskController::SixteenSector;
	}

	return target;
}

Analyser::Static::Target *OricTarget(const Storage::Encodings::AppleGCR::Sector *sector_zero) {
	using Target = Analyser::Static::Oric::Target;
	auto *const target = new Target;
	target->rom = Target::ROM::Pravetz;
	target->disk_interface = Target::DiskInterface::Pravetz;
	target->loading_command = "CALL 800\n";
	return target;
}

}

Analyser::Static::TargetList Analyser::Static::DiskII::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// Grab track 0, sector 0: the boot sector.
	const auto track_zero = media.disks.front()->get_track_at_position(Storage::Disk::Track::Address(0, Storage::Disk::HeadPosition(0)));
	const auto sector_map = Storage::Encodings::AppleGCR::sectors_from_segment(
		Storage::Disk::track_serialisation(*track_zero, Storage::Time(1, 50000)));

	const Storage::Encodings::AppleGCR::Sector *sector_zero = nullptr;
	for(const auto &pair: sector_map) {
		if(!pair.second.address.sector) {
			sector_zero = &pair.second;
			break;
		}
	}

	// If there's no boot sector then if there are also no sectors at all,
	// decline to nominate a machine. Otherwise go with an Apple as the default.
	TargetList targets;
	if(!sector_zero) {
		if(sector_map.empty()) {
			return targets;
		} else {
			targets.push_back(std::unique_ptr<Analyser::Static::Target>(AppleTarget(nullptr)));
			targets.back()->media = media;
			return targets;
		}
	}

	// If the boot sector looks like it's intended for the Oric, create an Oric.
	// Otherwise go with the Apple II.

	const auto disassembly = Analyser::Static::MOS6502::Disassemble(sector_zero->data, Analyser::Static::Disassembler::OffsetMapper(0xb800), {0xb800});

	bool did_read_shift_register = false;
	bool is_oric = false;

	// Look for a tight BPL loop reading the Oric's shift register address of 0x31c. The Apple II just has RAM there,
	// so the probability of such a loop is infinitesimal.
	for(const auto &instruction: disassembly.instructions_by_address) {
		// Is this a read of the shift register?
		if(
			(
				(instruction.second.operation == Analyser::Static::MOS6502::Instruction::LDA) ||
				(instruction.second.operation == Analyser::Static::MOS6502::Instruction::LDX) ||
				(instruction.second.operation == Analyser::Static::MOS6502::Instruction::LDY)
			) &&
			instruction.second.addressing_mode == Analyser::Static::MOS6502::Instruction::Absolute &&
			instruction.second.address == 0x031c) {
			did_read_shift_register = true;
			continue;
		}

		if(did_read_shift_register) {
			if(
				instruction.second.operation == Analyser::Static::MOS6502::Instruction::BPL &&
				instruction.second.address == 0xfb) {
				is_oric = true;
				break;
			}

			did_read_shift_register = false;
		}
	}

	// Check also for calls into the 0x3xx page above 0x320, as that's where the Oric's boot ROM is.
	for(const auto address: disassembly.outward_calls) {
		is_oric |= address >= 0x320 && address < 0x400;
	}

	if(is_oric) {
		targets.push_back(std::unique_ptr<Analyser::Static::Target>(OricTarget(sector_zero)));
	} else {
		targets.push_back(std::unique_ptr<Analyser::Static::Target>(AppleTarget(sector_zero)));
	}
	targets.back()->media = media;
	return targets;
}
