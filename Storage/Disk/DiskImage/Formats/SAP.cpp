//
//  SAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "SAP.hpp"

#include "Numeric/CRC.hpp"

using namespace Storage::Disk;

SAP::SAP(const std::string &file_name) : file_(file_name) {
	// Header:
	//
	//	1 byte: disk geometry.
	//		format 0 => 80 tracks, 256-byte sectors, 16 sectors/track;
	//		format 1 => 40 tracks, 128-byte sectors, 16 sectors/track.
	//	65 bytes: text signature.
	//
	sector_size_ = file_.get();
	if(sector_size_ != 1 && sector_size_ != 2) {
		throw Error::InvalidFormat;
	}

	// Convert to regularised IBM-style floppy disk form.
	sector_size_ = sector_size_ == 2 ? 0 : 1;

	// Test only the start of the signature.
	if(!file_.check_signature<SignatureType::String>("SYSTEME D'ARCHIVAGE PUKALL S.A.P.")) {
		throw Error::InvalidFormat;
	}
	file_.seek(66, Whence::SET);

	// Sectors from here: 4 bytes address, then all data, then a CRC16 of the data section.
	// On-disk data bytes are XORd with 0xb3 for some reason.
	while(true) {
		const auto format = file_.get();
		const auto protection = file_.get();
		const auto track = file_.get();
		const auto sector = file_.get();
		if(file_.eof()) break;
		printf("format %d protection %d track %d sector %d; ", format, protection, track, sector);

		file_.seek(-4, Whence::CUR);
		auto contents = file_.read(256 + 4);
		for(size_t c = 4; c < contents.size(); c++) {
			contents[c] ^= 0xb3;
		}
		const auto calculated = CRC::Generator<uint16_t, 0x1021, 0xffff, 0x0000, true, true>::crc_of(contents);

		const auto crc = file_.get_be<uint16_t>();
		printf("CRC: %04x [%04x]\n", crc, calculated);
	}

	printf("---\n");
}

HeadPosition SAP::maximum_head_position() const {
	return HeadPosition(2);
}

bool SAP::is_read_only() const {
	return true;
}

bool SAP::represents(const std::string &name) const {
	return name == file_.name();
}

Track::Address SAP::canonical_address(const Track::Address address) const {
	return Track::Address(
		address.head,
		HeadPosition(address.position.as_int())
	);
}

std::unique_ptr<Track> SAP::track_at_position(Track::Address) const {
	return nullptr;
}

void SAP::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &) {
}
