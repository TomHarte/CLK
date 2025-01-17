//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../../../Storage/Disk/Parsers/CPM.hpp"
#include "../../../Storage/Disk/Encodings/MFM/Parser.hpp"
#include "../../../Storage/Tape/Parsers/Spectrum.hpp"

#include "Target.hpp"

#include <algorithm>

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
	Storage::Encodings::MFM::Parser parser(Storage::Encodings::MFM::Density::Double, disk);

	// Grab absolutely any sector from the first track to determine general encoding.
	const Storage::Encodings::MFM::Sector *any_sector = parser.any_sector(0, 0);
	if(!any_sector) return false;

	// Determine the sector base and get logical sector 1.
	const uint8_t sector_base = any_sector->address.sector & 0xc0;
	const Storage::Encodings::MFM::Sector *boot_sector = parser.sector(0, 0, sector_base + 1);
	if(!boot_sector) return false;

	Storage::Disk::CPM::ParameterBlock cpm_format{};
	switch(sector_base) {
		case 0x40:	cpm_format = Storage::Disk::CPM::ParameterBlock::cpc_system_format();	break;
		case 0xc0:	cpm_format = Storage::Disk::CPM::ParameterBlock::cpc_data_format();		break;

		default: {
			// Check the first ten bytes of the first sector for the disk format; if these are all
			// the same value then instead substitute a default format.
			std::array<uint8_t, 10> format;
			std::copy(boot_sector->samples[0].begin(), boot_sector->samples[0].begin() + 10, format.begin());
			if(std::all_of(format.begin(), format.end(), [&](const uint8_t v) { return v == format[0]; })) {
				format = {0x00, 0x00, 0x28, 0x09, 0x02, 0x01, 0x03, 0x02, 0x2a, 0x52};
			}

			// Parse those ten bytes as:
			//
			// Byte 0: disc type
			// Byte 1: sidedness
			//		bits 0-6: arrangement
			//			0 => single sided
			//			1 => double sided, flip sides
			//			2 => double sided, up and over
			//		bit 7: double-track
			// Byte 2: number of tracks per side
			// Byte 3: number of sectors per track
			// Byte 4: Log2(sector size) - 7
			// Byte 5: number of reserved tracks
			// Byte 6: Log2(block size) - 7
			// Byte 7: number of directory blocks
			// Byte 8: gap length (read/write)
			// Byte 9: gap length(format)
			cpm_format.sectors_per_track = format[3];
			cpm_format.tracks = format[2];
			cpm_format.block_size = 128 << format[6];
			cpm_format.first_sector = sector_base + 1;
			cpm_format.reserved_tracks = format[5];

			// i.e. bits set downward from 0x4000 for as many blocks as form the catalogue.
			cpm_format.catalogue_allocation_bitmap = 0x8000 - (0x8000 >> format[7]);
		} break;
	}

	// If the boot sector sums to 3 modulo 256 then this is a Spectrum disk.
	const auto byte_sum = static_cast<uint8_t>(
		std::accumulate(boot_sector->samples[0].begin(), boot_sector->samples[0].end(), 0));
	if(byte_sum == 3) {
		return true;
	}

	// ... otherwise read a CPM directory and look for a BASIC program called "DISK".
	const auto catalogue = Storage::Disk::CPM::GetCatalogue(disk, cpm_format, false);
	return catalogue && catalogue->is_zx_spectrum_booter();
}

}

Analyser::Static::TargetList Analyser::Static::ZXSpectrum::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
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
