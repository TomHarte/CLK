//
//  DiskCopy42.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MacintoshIMG.hpp"

#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/Disk/Track/TrackSerialiser.hpp"
#include "Storage/Disk/Encodings/AppleGCR/Encoder.hpp"
#include "Storage/Disk/Encodings/AppleGCR/SegmentParser.hpp"

#include <algorithm>
#include <bit>
#include <cstring>

/*
	File format specifications as referenced below are largely
	sourced from the documentation at
	https://wiki.68kmla.org/DiskCopy_4.2_format_specification
*/

using namespace Storage::Disk;

MacintoshIMG::MacintoshIMG(
	const std::string &file_name,
	const FixedType type,
	const size_t offset,
	const size_t length
) :
	file_(file_name) {

	switch(type) {
		case FixedType::GCR:
			construct_raw_gcr(offset, length);
		break;
		default:
			throw Error::InvalidFormat;
	}
}

MacintoshIMG::MacintoshIMG(const std::string &file_name) :
	file_(file_name) {

	// Test 1: is this a raw sector dump? If so it'll start with
	// either the magic word 0x4C4B (big endian) or with 0x0000
	// and be exactly 819,200 bytes long if double sided, or
	// 409,600 bytes if single sided.
	//
	// Luckily, both 0x00 and 0x4c are invalid string length for the proper
	// DiskCopy 4.2 format, so there's no ambiguity here.
	const auto name_length = file_.get();
	if(name_length == 0x4c || !name_length) {
		const uint32_t magic_word = file_.get();
		if(!((name_length == 0x4c && magic_word == 0x4b) || (name_length == 0x00 && magic_word == 0x00)))
			throw Error::InvalidFormat;

		construct_raw_gcr(0);
	} else {
		// DiskCopy 4.2 it is then:
		//
		// File format starts with 64 bytes dedicated to the disk name;
		// this is a Pascal-style string though there is apparently a
		// bug in one version of Disk Copy that can cause the length to
		// be one too high.
		//
		// Validate the length, then skip the rest of the string.
		is_diskCopy_file_ = true;
		if(name_length > 64) {
			throw Error::InvalidFormat;
		}

		// Get the length of the data and tag blocks.
		file_.seek(64, Whence::SET);
		const auto data_block_length = file_.get_be<uint32_t>();
		const auto tag_block_length = file_.get_be<uint32_t>();
		const auto data_checksum = file_.get_be<uint32_t>();
		const auto tag_checksum = file_.get_be<uint32_t>();

		// Don't continue with no data.
		if(!data_block_length) {
			throw Error::InvalidFormat;
		}

		// Check that this is a comprehensible disk encoding.
		const auto encoding = file_.get();
		switch(encoding) {
			default: throw Error::InvalidFormat;

			case 0:	encoding_ = Encoding::GCR400;	break;
			case 1:	encoding_ = Encoding::GCR800;	break;
			case 2:	encoding_ = Encoding::MFM720;	break;
			case 3:	encoding_ = Encoding::MFM1440;	break;
		}
		format_ = file_.get();

		// Check the magic number.
		const auto magic_number = file_.get_be<uint16_t>();
		if(magic_number != 0x0100)
			throw Error::InvalidFormat;

		// Read the data and tags, and verify that enough data
		// was present.
		data_ = file_.read(data_block_length);
		tags_ = file_.read(tag_block_length);

		if(data_.size() != data_block_length || tags_.size() != tag_block_length)
			throw Error::InvalidFormat;

		// Verify the two checksums.
		const auto computed_data_checksum = checksum(data_);
		const auto computed_tag_checksum = checksum(tags_, 12);

		/*
			Yuck! It turns out that at least some disk images have incorrect checksums,
			and other emulators accept them regardless. So this test is disabled, at least
			for now. It'd probably be smarter to accept the disk image as provisionally
			incorrect and somehow communicate the issue to the user? Or, much better,
			verify the filesystem if the checksums don't match.
		*/
		(void)data_checksum;
		(void)computed_data_checksum;
		(void)tag_checksum;
		(void)computed_tag_checksum;

//		if(computed_tag_checksum != tag_checksum || computed_data_checksum != data_checksum)
//			throw Error::InvalidFormat;
	}
}

void MacintoshIMG::construct_raw_gcr(const size_t offset, size_t size) {
	is_diskCopy_file_ = false;
	if(size == 0) {
		size = size_t(file_.stats().st_size);
	}
	if(size != 819200 && size != 409600) {
		throw Error::InvalidFormat;
	}

	raw_offset_ = long(offset);
	file_.seek(raw_offset_, Whence::SET);
	if(size == 819200) {
		encoding_ = Encoding::GCR800;
		format_ = 0x22;
		data_ = file_.read(819200);
	} else {
		encoding_ = Encoding::GCR400;
		format_ = 0x02;
		data_ = file_.read(409600);
	}
}

uint32_t MacintoshIMG::checksum(const std::vector<uint8_t> &data, const size_t bytes_to_skip) const {
	// Checksum algorithm: take each two bytes as a big-endian word; add that to a
	// 32-bit accumulator and then rotate the accumulator right one position.
	uint32_t result = 0;
	for(size_t c = bytes_to_skip; c < data.size(); c += 2) {
		result += uint32_t(data[c + 0] << 8);
		result += uint32_t(data[c + 1] << 0);
		result = std::rotr(result, 1);
	}

	return result;
}

HeadPosition MacintoshIMG::maximum_head_position() const {
	return HeadPosition(80);
}

int MacintoshIMG::head_count() const {
	// Bit 5 in the format field indicates whether this disk is double
	// sided, regardless of whether it is GCR or MFM.
	return 1 + ((format_ & 0x20) >> 5);
}

bool MacintoshIMG::is_read_only() const {
	return file_.is_known_read_only();
}

bool MacintoshIMG::represents(const std::string &name) const {
	return name == file_.name();
}

std::unique_ptr<Track> MacintoshIMG::track_at_position(const Track::Address address) const {
	/*
		The format_ byte has the following meanings:

		GCR:
			This byte appears on disk as the GCR format nibble in every sector tag.
			The low five bits are an interleave factor, either:

				'2' for 0 8 1 9 2 10 3 11 4 12 5 13 6 14 7 15; or
				'4' for 0 4 8 12 1 5 9 13 2 6 10 14 3 7 11 15.

			Bit 5 indicates double sided or not.

		MFM:
			The low five bits provide sector size as a multiple of 256 bytes.
			Bit 5 indicates double sided or not.
	*/

	const std::lock_guard buffer_lock(buffer_mutex_);
	if(encoding_ == Encoding::GCR400 || encoding_ == Encoding::GCR800) {
		// Perform a GCR encoding.
		const auto included_sectors =
			Storage::Encodings::AppleGCR::Macintosh::sectors_in_track(address.position.as_int());
		const size_t start_sector =
			size_t(included_sectors.start * head_count() + included_sectors.length * address.head);

		if(start_sector*512 >= data_.size()) return nullptr;
		const uint8_t *const sector = &data_[512 * start_sector];
		const uint8_t *const tags = tags_.size() ? &tags_[12 * start_sector] : nullptr;

		Storage::Disk::PCMSegment segment;
		segment += Encodings::AppleGCR::six_and_two_sync(24);

		// Determine the sector ordering.
		uint8_t source_sectors[12] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		int destination = 0;
		for(int c = 0; c < included_sectors.length; ++c) {
			// Deal with collisions by finding the next non-colliding spot.
			while(source_sectors[destination] != 0xff) destination = (destination + 1) % included_sectors.length;
			source_sectors[destination] = uint8_t(c);
			destination = (destination + (format_ & 0x1f)) % included_sectors.length;
		}

		for(int c = 0; c < included_sectors.length; ++c) {
			const uint8_t sector_id = source_sectors[c];
			uint8_t sector_plus_tags[524];
			auto target = std::begin(sector_plus_tags);

			// Copy in the tags, if provided; otherwise generate them.
			if(tags) {
				std::copy(&tags[sector_id * 12], &tags[(sector_id + 1) * 12], target);
			} else {
				// TODO: fill in tags properly.
				std::fill_n(target, 12, 0);
			}
			target += 12;

			// Copy in the sector body.
			std::copy(&sector[sector_id * 512], &sector[(sector_id + 1) * 512], target);

			// NB: sync lengths below are probably not identical to any
			// specific Mac.
			segment += Encodings::AppleGCR::six_and_two_sync(28);
			segment += Encodings::AppleGCR::Macintosh::header(
				format_,
				uint8_t(address.position.as_int()),
				sector_id,
				address.head > 0
			);
			segment += Encodings::AppleGCR::six_and_two_sync(4);
			segment += Encodings::AppleGCR::Macintosh::data(sector_id, sector_plus_tags);
		}

		// TODO: it seems some tracks are skewed respective to others; investigate further.

		return std::make_unique<PCMTrack>(segment);
	}

	return nullptr;
}

void MacintoshIMG::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks) {
	std::map<Track::Address, std::vector<uint8_t>> tracks_by_address;
	for(const auto &pair: tracks) {
		// Determine a data rate for the track.
		const auto included_sectors = Storage::Encodings::AppleGCR::Macintosh::sectors_in_track(pair.first.position.as_int());

		// Rule of thumb here: there are about 6250 bits per sector.
		const int data_rate = included_sectors.length * 6250;

		// Decode the track.
		const auto sector_map = Storage::Encodings::AppleGCR::sectors_from_segment(
			Storage::Disk::track_serialisation(*pair.second, Storage::Time(1, data_rate)));

		// Rearrange sectors into ascending order.
		std::vector<uint8_t> track_contents(size_t(524 * included_sectors.length));
		for(const auto &sector_pair: sector_map) {
			const size_t target_address = sector_pair.second.address.sector * 524;
			if(target_address >= track_contents.size() || sector_pair.second.data.size() != 524) continue;
			std::copy(
				sector_pair.second.data.begin(),
				sector_pair.second.data.begin() + 524,
				&track_contents[target_address]
			);
		}

		// Store for later.
		tracks_by_address[pair.first] = std::move(track_contents);
	}

	// Grab the buffer mutex and update the in-memory buffer.
	{
		std::lock_guard buffer_lock(buffer_mutex_);
		for(const auto &pair: tracks_by_address) {
			const auto included_sectors = Storage::Encodings::AppleGCR::Macintosh::sectors_in_track(pair.first.position.as_int());
			size_t start_sector = size_t(included_sectors.start * head_count() + included_sectors.length * pair.first.head);

			for(int c = 0; c < included_sectors.length; ++c) {
				const auto sector_plus_tags = &pair.second[size_t(c)*524];

				// Copy the 512 bytes that constitute the sector body.
				std::copy(&sector_plus_tags[12], &sector_plus_tags[12 + 512], &data_[start_sector * 512]);

				// Copy the tags if this file can store them.
				// TODO: add tags to a DiskCopy-format image that doesn't have them, if they contain novel content?
				if(tags_.size()) {
					std::copy(sector_plus_tags, sector_plus_tags + 12, &tags_[start_sector * 12]);
				}

				++start_sector;
			}
		}
	}

	// Grab the file lock and write out the new tracks.
	{
		std::lock_guard lock_guard(file_.file_access_mutex());

		if(!is_diskCopy_file_) {
			// Just dump out the entire disk. Grossly lazy, possibly worth improving.
			file_.seek(raw_offset_, Whence::SET);
			file_.write(data_);
		} else {
			// Write out the sectors, and possibly the tags, and update checksums.
			file_.seek(0x54, Whence::SET);
			file_.write(data_);
			file_.write(tags_);

			const auto data_checksum = checksum(data_);
			const auto tag_checksum = checksum(tags_, 12);

			file_.seek(0x48, Whence::SET);
			file_.put_be(data_checksum);
			file_.put_be(tag_checksum);
		}
	}
}
