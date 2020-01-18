//
//  STX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "STX.hpp"

#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Shifter.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/PCMTrack.hpp"

#include "Utility/ImplicitSectors.hpp"

#include <array>
#include <cstdlib>
#include <cstring>

using namespace Storage::Disk;

namespace {

class TrackConstructor {
	public:
		constexpr static uint16_t NoFirstOffset = std::numeric_limits<uint16_t>::max();

		struct Sector {
			// Records explicitly present in the sector table.
			uint32_t data_offset = 0;
			size_t bit_position = 0;
			uint16_t data_duration = 0;
			std::array<uint8_t, 6> address = {0, 0, 0, 0, 0, 0};
			uint8_t status = 0;

			// Other facts that will either be supplied by the STX or which
			// will be empty.
			std::vector<uint8_t> fuzzy_mask;
			std::vector<uint8_t> contents;
			std::vector<uint16_t> timing;

			// Accessors.

			/// @returns The byte size of this sector, according to its address mark.
			uint32_t data_size() const {
				return uint32_t(128 << (address[3]&3));
			}

			struct Fragment {
				int prior_syncs = 1;
				std::vector<uint8_t> contents;
			};

			/// @returns The byte stream this sector address would produce if a WD read track command were to observe it.
			std::vector<Fragment> get_track_address_fragments() const {
				return track_fragments(address.begin(), address.begin() + 4, {0xa1, 0xa1, 0xfe});
			}

			/// @returns The byte stream this sector data would produce if a WD read track command were to observe it.
			std::vector<Fragment> get_track_data_fragments() const {
				return track_fragments(contents.begin(), contents.end(), {0xa1, 0xa1, 0xfb});
			}

			/*!
				Acts like std::search except that it tries to find a start location from which all of the members of @c fragments
				can be found in successive order with no more than a 'permissible' amount of gap between them.

				Where 'permissible' is derived empirically from trial and error; in practice it's a measure of the number of bytes
				a WD may produce when it has encountered a false sync, and I don't have documentation on that. So it's
				derived from in-practice testing of STXs (which, hopefully, contain an accurate copy of what a WD would do,
				so are themselves possibly a way to research that).
			*/
			template <typename Iterator> static Iterator find_fragments(Iterator begin, Iterator end, const std::vector<Fragment> &fragments) {
				while(true) {
					// To match the fragments, they must all be found, in order, with at most two bytes of gap.
					auto this_begin = begin;
					std::vector<uint8_t>::const_iterator first_location = end;
					bool is_found = true;
					bool is_first = true;
					for(auto fragment: fragments) {
						auto location = std::search(this_begin, end, fragment.contents.begin(), fragment.contents.end());

						// If fragment wasn't found at all, it's never going to be found. So game over.
						if(location == end) {
							return location;
						}

						// Otherwise, either mark
						if(is_first) {
							first_location = location;
						} else if(location > this_begin + 5*fragment.prior_syncs) {
							is_found = false;
							break;
						}

						is_first = false;
						this_begin = location + ssize_t(fragment.contents.size());
					}

					if(is_found) {
						return first_location;
					}

					// TODO: can I assume more than this?
					++begin;
				}
				return end;
			}

			private:
				/// @returns The effect of encoding @c prefix followed by the bytes from @c begin to @c end as MFM data and then decoding them as if
				/// observed by a WD read track command, split into fragments separated by any instances of false sync — since it's still unclear to me exactly what
				/// a WD should put out in those instances.
				template <typename T> static std::vector<Fragment> track_fragments(T begin, T end, std::initializer_list<uint8_t> prefix) {
					std::vector<Fragment> result;
					result.reserve(size_t(end - begin) + prefix.size());

					PCMSegment segment;
					std::unique_ptr<Storage::Encodings::MFM::Encoder> encoder = Storage::Encodings::MFM::GetMFMEncoder(segment.data);

					// Encode prefix.
					for(auto c: prefix) {
						encoder->add_byte(c);
					}

					// Encode body.
					while(begin != end) {
						encoder->add_byte(*begin);
						++begin;
					}

					// Decode, starting a new segment upon any false sync since I don't have good documentation
					// presently on exactly how a WD should react to those.
					using Shifter = Storage::Encodings::MFM::Shifter;
					Shifter shifter;
					shifter.set_should_obey_syncs(true);
					shifter.set_is_double_density(true);

					result.emplace_back();

					// Add whatever comes from the track.
					int ignore_count = 0;
					for(auto bit: segment.data) {
						shifter.add_input_bit(int(bit));

						const auto token = shifter.get_token();
						if(token != Shifter::None) {
							if(ignore_count) {
								--ignore_count;
								continue;
							}

							// If anything other than a byte is encountered,
							// skip it and the next thing to be reported,
							// beginning a new fragment.
							if(token != Shifter::Token::Byte) {
								ignore_count = 1;

								if(!result.back().contents.empty()) {
									result.emplace_back();
								} else {
									++result.back().prior_syncs;
								}

								continue;
							}

							// This was an ordinary byte, retain it.
							result.back().contents.push_back(shifter.get_byte());
						}
					}

					return result;
				}
		};


		TrackConstructor(const std::vector<uint8_t> &track_data, const std::vector<Sector> &sectors, size_t track_size, uint16_t first_sync) :
			track_data_(track_data), sectors_(sectors), track_size_(track_size), first_sync_(first_sync) {
		}

		std::shared_ptr<PCMTrack> get_track() {
			// If no contents are supplied, return an unformatted track.
			if(sectors_.empty() && track_data_.empty()) {
				return nullptr;
			}

			// If no sectors are on this track, just encode the track data. STX allows speed
			// changes and fuzzy bits in sectors only.
			if(sectors_.empty()) {
				PCMSegment segment;
				std::unique_ptr<Storage::Encodings::MFM::Encoder> encoder = Storage::Encodings::MFM::GetMFMEncoder(segment.data);
				for(auto c: track_data_) {
					encoder->add_byte(c);
				}
				return std::make_shared<PCMTrack>(segment);
			}

			// Otherwise, seek to encode the sectors, using the track data to
			// fill in the gaps (if provided).
			std::unique_ptr<Storage::Encodings::MFM::Encoder> encoder;
			std::unique_ptr<PCMSegment> segment;

			// To reconcile the list of sectors with the WD get track-style track image,
			// use sector bodies as definitive and refer to the track image for in-fill.
			auto track_position = track_data_.begin();
			const auto sync_mark = {0xa1, 0xa1};
			struct Location {
				enum Type {
					Address, Data
				} type;
				std::vector<uint8_t>::const_iterator position;
				const Sector &sector;

				Location(Type type, std::vector<uint8_t>::const_iterator position, const Sector &sector) : type(type), position(position), sector(sector) {}
			};
			std::vector<Location> locations;
			for(const auto &sector: sectors_) {
				{
					// Find out what the address would look like, if found in a read track.
					const auto address_fragments = sector.get_track_address_fragments();

					// Try to locate the header within the track image; if it can't be found then settle for
					// the next thing that looks like a header of any sort.
					auto address_position = TrackConstructor::Sector::find_fragments(track_position, track_data_.end(), address_fragments);
					if(address_position == track_data_.end()) {
						address_position = std::search(track_position, track_data_.end(), sync_mark.begin(), sync_mark.end());
					}

					// Place this address only if somewhere to put it was found.
					if(address_position != track_data_.end()) {
						locations.emplace_back(Location::Address, address_position, sector);

						// Advance the track position.
						track_position = address_position + 6;
					}
				}

				// Do much the same thing for the data, if it exists.
				if(!(sector.status & 0x10)) {
					const auto data_fragments = sector.get_track_data_fragments();

					auto data_position = TrackConstructor::Sector::find_fragments(track_position, track_data_.end(), data_fragments);
					if(data_position == track_data_.end()) {
						data_position = std::search(track_position, track_data_.end(), sync_mark.begin(), sync_mark.end());
					}
					if(data_position == track_data_.end()) {
						// Desperation: guess from the given offset.
						data_position = track_data_.begin() + (sector.bit_position / 16);
					}

					locations.emplace_back(Location::Data, data_position, sector);
					track_position = data_position + sector.data_size();
				}
			}

			// Just create an encoder if one doesn't exist. TODO: factor in data rate.
			if(!encoder) {
				segment.reset(new PCMSegment);
				encoder = Storage::Encodings::MFM::GetMFMEncoder(segment->data);
			}

			// Write out, being wary of potential overlapping sectors, and copying from track_data_ to fill in gaps.
			auto location = locations.begin();
			track_position = track_data_.begin();
			while(location != locations.end()) {
//				assert(location->position >= track_position && location->position < track_data_.end());

				// Advance to location.position.
				while(track_position < location->position) {
					encoder->add_byte(*track_position);
					++track_position;
				}

				// Write the relevant mark and fill in a default number of bytes to write.
				size_t bytes_to_write;
				switch(location->type) {
					default:
					case Location::Address:
						encoder->add_ID_address_mark();
						bytes_to_write = 6;
					break;
					case Location::Data:
						if(location->sector.status & 0x20)
							encoder->add_deleted_data_address_mark();
						else
							encoder->add_data_address_mark();
						bytes_to_write = location->sector.data_size() + 2;
					break;
				}
				track_position += 3;

				// Decide how much data to write for real; this [partially] allows for overlapping sectors.
				auto next_location = location + 1;
				if(next_location != locations.end()) {
					bytes_to_write = std::min(bytes_to_write, size_t(next_location->position - track_position));
				}

				// Skip that many bytes from the underlying track image.
				track_position += ssize_t(bytes_to_write);

				// Write bytes.
				switch(location->type) {
					default:
					case Location::Address:
						for(size_t c = 0; c < bytes_to_write; ++c)
							encoder->add_byte(location->sector.address[c]);
					break;
					case Location::Data: {
						const auto body_bytes = std::min(bytes_to_write, size_t(location->sector.data_size()));
						for(size_t c = 0; c < body_bytes; ++c)
							encoder->add_byte(location->sector.contents[c]);

						// Add a CRC only if it fits (TODO: crop if necessary?).
						if(bytes_to_write & 127) {
							encoder->add_crc((location->sector.status & 0x18) == 0x10);
						}
					} break;
				}

				// Advance location.
				++location;
			}

			// Write anything remaining from the track image.
			while(track_position < track_data_.end()) {
				encoder->add_byte(*track_position);
				++track_position;
			}

			// Write generic padding up until the specified track size.
			while(segment->data.size() < track_size_ * 16) {
				encoder->add_byte(0x4e);
			}

			// Pad out to the minimum size a WD can actually make sense of.
			// I've no idea why it's valid for tracks to be shorter than this,
			// so likely I'm suffering a comprehansion deficiency.
			// TODO: determine why this isn't correct (or, possibly, is).
			while(segment->data.size() < 5750 * 16) {
				encoder->add_byte(0x4e);
			}

			return std::make_shared<PCMTrack>(*segment);
		}

	private:
		const std::vector<uint8_t> &track_data_;
		const std::vector<Sector> &sectors_;
		const size_t track_size_;
		const uint16_t first_sync_;

};

}

STX::STX(const std::string &file_name) : file_(file_name) {
	// Require that this be a version 3 Pasti.
	if(!file_.check_signature("RSY", 4)) throw Error::InvalidFormat;
	if(file_.get16le() != 3) throw Error::InvalidFormat;

	// Skip: tool used, 2 reserved bytes.
	file_.seek(4, SEEK_CUR);

	// Skip the track count, test for a new-style encoding, skip a reserved area.
	file_.seek(1, SEEK_CUR);
	is_new_format_ = file_.get8() == 2;
	file_.seek(4, SEEK_CUR);

	// Set all tracks absent.
	memset(offset_by_track_, 0, sizeof(offset_by_track_));

	// Parse the tracks table to fill in offset_by_track_. The only available documentation
	// for STX is unofficial and makes no promise about track order. Hence the bucket sort,
	// effectively putting them into track order.
	//
	//	Track descriptor layout:
	//
	//	0	4	Record size.
	//	4	4	Number of bytes in fuzzy mask record.
	//	8	2	Number of sectors on track.
	//	10	2	Track flags.
	//	12	2	Total number of bytes on track.
	//	14	1	Track number (b7 = side, b0-b6 = track).
	//	15	1	Track type.
	track_count_ = 0;
	head_count_ = 1;
	while(true) {
		const long offset = file_.tell();
		const uint32_t size = file_.get32le();
		if(file_.eof()) break;

		// Skip fields other than track position, then fill in table position and advance.
		file_.seek(10, SEEK_CUR);

		const uint8_t track_position = file_.get8();
		offset_by_track_[track_position] = offset;

		// Update the maximum surface dimensions.
		track_count_ = std::max(track_count_, track_position & 0x7f);
		head_count_ = std::max(head_count_, ((track_position & 0x80) >> 6));

		// Seek next track start.
		file_.seek(offset + size, SEEK_SET);
	}
}

HeadPosition STX::get_maximum_head_position() {
	return HeadPosition(track_count_ + 1);	// Same issue as MSA; must fix!
}

int STX::get_head_count() {
	return head_count_;
}

std::shared_ptr<::Storage::Disk::Track> STX::get_track_at_position(::Storage::Disk::Track::Address address) {
	// These images have two sides, at most.
	if(address.head > 1) return nullptr;

	// If no track was found, there's nothing to do here.
	const int track_index = (address.head * 0x80) + address.position.as_int();
	if(!offset_by_track_[track_index]) return nullptr;

	// Seek to the track (skipping the record size field).
	file_.seek(offset_by_track_[track_index] + 4, SEEK_SET);

	// Grab the track description.
	const uint32_t fuzzy_size = file_.get32le();
	const uint16_t sector_count = file_.get16le();
	const uint16_t flags = file_.get16le();
	const size_t track_length = file_.get16le();
	file_.seek(2, SEEK_CUR);		// Skip track type; despite being named, it's apparently unused.

	// If this is a trivial .ST-style sector dump, life is easy.
	if(!(flags & 1)) {
		const auto sector_contents = file_.read(sector_count * 512);
		return track_for_sectors(sector_contents.data(), sector_count, uint8_t(address.position.as_int()), uint8_t(address.head), 1, 2, true);
	}

	// Grab sector records, if provided.
	std::vector<TrackConstructor::Sector> sectors;
	std::vector<uint8_t> track_data;
	uint16_t first_sync = TrackConstructor::NoFirstOffset;

	// Sector records come first.
	for(uint16_t c = 0; c < sector_count; ++c) {
		sectors.emplace_back();
		sectors.back().data_offset = file_.get32le();
		sectors.back().bit_position = file_.get16le();
		sectors.back().data_duration = file_.get16le();
		file_.read(sectors.back().address);
		sectors.back().status = file_.get8();
		file_.seek(1, SEEK_CUR);
	}

	// If fuzzy masks are specified, attach them to their corresponding sectors.
	if(fuzzy_size) {
		uint32_t fuzzy_bytes_read = 0;
		for(auto &sector: sectors) {
			// Check for the fuzzy bit mask; if it's not set then
			// there's nothing for this sector.
			if(!(sector.status & 0x80)) continue;

			// Make sure there are enough bytes left.
			const uint32_t expected_bytes = sector.data_size();
			if(fuzzy_bytes_read + expected_bytes > fuzzy_size) break;

			// Okay, there are, so read them.
			sector.fuzzy_mask = file_.read(expected_bytes);
			fuzzy_bytes_read += expected_bytes;
		}

		// It should be true that the number of fuzzy masks caused
		// exactly the correct number of fuzzy bytes to be read.
		// But, just in case, check and possibly skip some.
		file_.seek(long(fuzzy_size) - fuzzy_bytes_read, SEEK_CUR);
	}

	// There may or may not be a track image. Grab it if so.

	// Grab the read-track-esque track contents, if available.
	long sector_start = file_.tell();
	if(flags & 0x40) {
		// Bit 6 => there is a track to read;
		// bit
		if(flags & 0x80) {
			first_sync = file_.get16le();
			const uint16_t image_size = file_.get16le();
			track_data = file_.read(image_size);
		} else {
			const uint16_t image_size = file_.get16le();
			track_data = file_.read(image_size);
		}
	}

	// Grab sector contents.
	long end_of_data = file_.tell();
	for(auto &sector: sectors) {
		// If the FDC record-not-found flag is set, there's no sector body to find.
		// Otherwise there's a sector body in the file somewhere.
		if(!(sector.status & 0x10)) {
			file_.seek(sector.data_offset + sector_start, SEEK_SET);
			sector.contents = file_.read(sector.data_size());
			end_of_data = std::max(end_of_data, file_.tell());
		}
	}
	file_.seek(end_of_data, SEEK_SET);

	// Grab timing info if available.
	file_.seek(4, SEEK_CUR);	// Skip the timing descriptor, as it includes no new information.
	for(auto &sector: sectors) {
		// Skip any sector with no intra-sector bit width variation.
		if(!(sector.status&1)) continue;

		const auto timing_record_size = sector.data_size() >> 4;	// Use one entry per 16 bytes.
		sector.timing.resize(timing_record_size);

		if(!is_new_format_) {
			// Generate timing records for Macrodos/Speedlock.
			// Timing is specified in quarters. Which might or might not be
			// quantities of 128 bytes, who knows?
			for(size_t c = 0; c < timing_record_size; ++c) {
				if(c < (timing_record_size >> 2)) {
					sector.timing[c] = 127;
				} else if(c < ((timing_record_size*2) >> 2)) {
					sector.timing[c] = 133;
				} else if(c < ((timing_record_size*3) >> 2)) {
					sector.timing[c] = 121;
				} else {
					sector.timing[c] = 127;
				}
			}

			continue;
		}

		// This is going to be a new-format record.
		for(size_t c = 0; c < timing_record_size; ++c) {
			sector.timing[c] = file_.get16be();		// These values are big endian, unlike the rest of the file.
		}
	}

	// Sort the sectors by starting position. It's perfectly possible that they're always
	// sorted in STX but, again, the reverse-engineered documentation doesn't make the
	// promise, so that's that.
	std::sort(sectors.begin(), sectors.end(),
		[] (TrackConstructor::Sector &lhs, TrackConstructor::Sector &rhs) {
			return lhs.bit_position < rhs.bit_position;
	});


	/*
		Having reached here, the actual stuff of parsing the file structure should be done.
		So hand off to the TrackConstructor.

	*/

	TrackConstructor constructor(track_data, sectors, track_length, first_sync);
	return constructor.get_track();
}
