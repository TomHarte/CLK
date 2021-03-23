//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../../../Storage/Disk/Encodings/MFM/Parser.hpp"
#include "../../../Storage/Tape/Parsers/Spectrum.hpp"

#include "Target.hpp"

namespace {

bool IsSpectrumTape(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	Parser parser(Parser::MachineType::ZXSpectrum);

	while(true) {
		const auto block = parser.find_block(tape);
		if(!block) break;

		// Check for a Spectrum header block.
		if(block->type == 0x00) {
			return true;
		}
	}

	return false;
}

bool IsSpectrumDisk(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Get logical sector 1; the Spectrum appears to support various physical
	// sectors as sector 1.
	Storage::Encodings::MFM::Sector *boot_sector = nullptr;
	uint8_t sector_mask = 0;
	while(!boot_sector) {
		boot_sector = parser.get_sector(0, 0, sector_mask + 1);
		sector_mask += 0x40;
		if(!sector_mask) break;
	}
	if(!boot_sector) return false;

	// Test that the contents of the boot sector sum to 3, modulo 256.
	uint8_t byte_sum = 0;
	for(auto byte: boot_sector->samples[0]) {
		byte_sum += byte;
	}
	return byte_sum == 3;
}

}

Analyser::Static::TargetList Analyser::Static::ZXSpectrum::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	TargetList destination;
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;

	if(!media.tapes.empty()) {
		bool has_spectrum_tape = false;
		for(auto &tape: media.tapes) {
			has_spectrum_tape |= IsSpectrumTape(tape);
		}

		if(has_spectrum_tape) {
			target->media.tapes = media.tapes;
		}
	}

	if(!media.disks.empty()) {
		bool has_spectrum_disk = false;

		for(auto &disk: media.disks) {
			has_spectrum_disk |= IsSpectrumDisk(disk);
		}

		if(has_spectrum_disk) {
			target->media.disks = media.disks;
			target->model = Target::Model::Plus3;
		}
	}

	// If any media survived, add the target.
	if(!target->media.empty()) {
		target->should_hold_enter = true;	// To force entry into the 'loader' and thereby load the media.
		destination.push_back(std::move(target));
	}

	return destination;
}
