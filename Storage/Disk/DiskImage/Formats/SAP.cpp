//
//  SAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "SAP.hpp"

#include "Numeric/CRC.hpp"
#include "Storage/Disk/Encodings/MFM/Encoder.hpp"

#include <vector>

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
}

HeadPosition SAP::maximum_head_position() const {
	// In the SAP file format, this is coupled to sector size.
	return HeadPosition(sector_size_ ? 80 : 40);
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

std::unique_ptr<Track> SAP::track_at_position(const Track::Address address) const {
	static constexpr int sectors_per_track = 16;
	const auto track_size = sectors_per_track * ((128 << sector_size_) + 6);
	const auto header_size = 66;
	file_.seek(address.position.as_int() * track_size + header_size, Whence::SET);

	std::vector<Encodings::MFM::Sector> sectors;
	sectors.reserve(sectors_per_track);

	for(int s = 0; s < sectors_per_track; s++) {
		auto &sector = sectors.emplace_back();

		[[maybe_unused]] const auto format = file_.get();
		[[maybe_unused]] const auto protection = file_.get();
		sector.address.track = file_.get();
		sector.address.sector = file_.get();
		sector.size = sector_size_;

		// SAP uses almost-but-not-quite an ordinary CCITT CRC.
		file_.seek(-4, Whence::CUR);
		auto contents = file_.read(256 + 4);
		for(size_t c = 4; c < contents.size(); c++) {
			contents[c] ^= 0xb3;
		}
		const auto calculated = CRC::Generator<uint16_t, 0x1021, 0xffff, 0x0000, true, true>::crc_of(contents);
		const auto crc = file_.get_be<uint16_t>();

		contents.erase(contents.begin(), contents.begin() + 4);
		sector.samples.push_back(contents);
		sector.has_data_crc_error = crc != calculated;
	}

	return Encodings::MFM::TrackWithSectors(Encodings::MFM::Density::Double, sectors);
}

void SAP::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &) {}
