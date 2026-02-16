//
//  MOOF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MOOF.hpp"

#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Numeric/CRC.hpp"

using namespace Storage::Disk;

namespace {
constexpr uint32_t chunk(const char *str) {
	return uint32_t(str[0] | (str[1] << 8) | (str[2] << 16) | (str[3] << 24));
}

class MOOFFluxTrack: public Track {
public:
	MOOFFluxTrack(std::vector<uint8_t> &&data, const uint32_t bit_count) :
		data_(std::move(data)), bit_count_(bit_count) {
		seek_to(0.0f);
	}

	Event get_next_event() final {
		Event event;
		if(data_iterator_ == data_.end() || bits_passed_ == bit_count_) {
			data_iterator_ = data_.begin();
			bits_passed_ = 0;
			return Event{
				.type = Storage::Disk::Track::Event::IndexHole,
				.length = Storage::Time()
			};
		}

		Event result;
		result.type = Storage::Disk::Track::Event::FluxTransition;
		result.length.clock_rate = 8'000'000;  // i.e. 125ns

		result.length.length = 0;
		while(true) {
			const auto delta = *data_iterator_;
			result.length.length += delta;
			++data_iterator_;

			if(delta != 0xff || data_iterator_ == data_.end()) {
				break;
			}
		}

		++bits_passed_;
		return result;
	}

	float seek_to(const float time_since_index_hole) final {
		bits_passed_ = 0;
		data_iterator_ = data_.begin();

		float time_observed = 0.0f;
		while(time_observed < time_since_index_hole) {
			auto prior = data_iterator_;
			const auto next_time = get_next_event().length.get<float>();

			if(time_observed + next_time > time_since_index_hole) {
				--bits_passed_;
				data_iterator_ = prior;
				break;
			}

			time_observed += next_time;
		}

		return time_observed;
	}

	Track *clone() const final {
		// TODO.
		// Luckily: won't be used until MOOFs stop being read-only.
		return nullptr;
	}

private:
	std::vector<uint8_t> data_;
	uint32_t bit_count_ = 0;

	std::vector<uint8_t>::iterator data_iterator_;
	uint32_t bits_passed_ = 0;
};
}

MOOF::MOOF(const std::string &file_name) :
	file_(file_name) {

	static constexpr char signature[] = {
		'M', 'O', 'O', 'F',
		char(0xff), 0x0a, 0x0d, 0x0a
	};
	if(!file_.check_signature<SignatureType::Binary>(signature)) {
		throw Error::InvalidFormat;
	}

	// Test the file's CRC32.
	const auto crc = file_.get_le<uint32_t>();
	post_crc_contents_ = file_.read(size_t(file_.stats().st_size - 12));
	const uint32_t computed_crc = CRC::CRC32::crc_of(post_crc_contents_);
	if(crc != computed_crc) {
		throw Error::InvalidFormat;
	}

	// Retreat to the first byte after the CRC and parse all chunks.
	file_.seek(12, Whence::SET);
	bool has_tmap = false, has_flux = false;
	std::fill(std::begin(track_map_), std::end(track_map_), 0xff);
	std::fill(std::begin(flux_map_), std::end(flux_map_), 0xff);
	while(true) {
		const auto chunk_id = file_.get_le<uint32_t>();
		const auto chunk_size = file_.get_le<uint32_t>();
		const auto end_of_chunk = file_.tell() + long(chunk_size);
		if(file_.eof()) break;

		switch(chunk_id) {
			case chunk("INFO"):
				info_.version = file_.get();
				info_.disk_type = Info::DiskType(file_.get());
				info_.is_write_protected = file_.get();
			break;

			case chunk("TMAP"):
				file_.read(track_map_, 160);
				has_tmap = true;
			break;

			case chunk("FLUX"):
				file_.read(flux_map_, 160);
				has_flux = true;
			break;

			case chunk("TRKS"):
				tracks_offset_ = file_.tell();
			break;


			default:
			break;
		}

		file_.seek(end_of_chunk, Whence::SET);
	}

	// Structural issues.
	if(tracks_offset_ == -1 || (!has_tmap && !has_flux)) {
		throw Error::InvalidFormat;
	}

	// Versioning.
	//
	// TODO: determine what it means to be a Twiggy disk, in encoding terms.
	const bool supports_disk_type =
		info_.disk_type == Info::DiskType::GCR400kb ||
		info_.disk_type == Info::DiskType::GCR800kb ||
		info_.disk_type == Info::DiskType::MFM;
	if(info_.version != 1 || !supports_disk_type) {
		throw Error::InvalidFormat;
	}
}

HeadPosition MOOF::maximum_head_position() const {
	return HeadPosition(80);
}

int MOOF::head_count() const {
	return info_.disk_type == Info::DiskType::GCR400kb ? 1 : 2;
}

std::unique_ptr<Track> MOOF::track_at_position(const Track::Address address) const {
	const int table_position = address.position.as_int() * 2 + address.head;
	if(table_position > 255) {
		return nullptr;
	}

	const auto location = [&](const uint8_t offset) -> TrackLocation {
		file_.seek(tracks_offset_ + 8 * long(offset), Whence::SET);
		TrackLocation location;
		location.starting_block = file_.get_le<uint16_t>();
		location.block_count = file_.get_le<uint16_t>();
		location.bit_count = file_.get_le<uint32_t>();
		return location;
	};

	if(flux_map_[table_position] != 0xff) {
		return flux(location(flux_map_[table_position]));
	} else if(track_map_[table_position] != 0xff) {
		return track(location(track_map_[table_position]));
	}

	return nullptr;
}

std::unique_ptr<Track> MOOF::flux(const TrackLocation location) const {
	file_.seek(location.starting_block * 512, Whence::SET);
	auto track_contents = file_.read((location.bit_count + 7) / 8);
	return std::make_unique<MOOFFluxTrack>(std::move(track_contents), location.bit_count);
}

std::unique_ptr<Track> MOOF::track(const TrackLocation location) const {
	file_.seek(location.starting_block * 512, Whence::SET);
	const auto track_contents = file_.read((location.bit_count + 7) / 8);
	return std::make_unique<PCMTrack>(PCMSegment(location.bit_count, track_contents));
}

bool MOOF::represents(const std::string &name) const {
	return name == file_.name();
}
