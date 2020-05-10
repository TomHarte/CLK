//
//  OricMFMDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "OricMFMDSK.hpp"

#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Shifter.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"

using namespace Storage::Disk;

OricMFMDSK::OricMFMDSK(const std::string &file_name) :
		file_(file_name) {
	if(!file_.check_signature("MFM_DISK"))
		throw Error::InvalidFormat;

	head_count_ = file_.get32le();
	track_count_ = file_.get32le();
	geometry_type_ = file_.get32le();

	if(geometry_type_ < 1 || geometry_type_ > 2)
		throw Error::InvalidFormat;
}

HeadPosition OricMFMDSK::get_maximum_head_position() {
	return HeadPosition(int(track_count_));
}

int OricMFMDSK::get_head_count() {
	return int(head_count_);
}

long OricMFMDSK::get_file_offset_for_position(Track::Address address) {
	int seek_offset = 0;
	switch(geometry_type_) {
		case 1:
			seek_offset = address.head * int(track_count_) + address.position.as_int();
		break;
		case 2:
			seek_offset = address.position.as_int() * int(track_count_ * head_count_) + address.head;
		break;
	}
	return long(seek_offset) * 6400 + 256;
}

std::shared_ptr<Track> OricMFMDSK::get_track_at_position(Track::Address address) {
	PCMSegment segment;
	{
		std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
		file_.seek(get_file_offset_for_position(address), SEEK_SET);

		// The file format omits clock bits. So it's not a genuine MFM capture.
		// A consumer must contextually guess when an FB, FC, etc is meant to be a control mark.
		std::size_t track_offset = 0;
		uint8_t last_header[6] = {0, 0, 0, 0, 0, 0};
		std::unique_ptr<Encodings::MFM::Encoder> encoder = Encodings::MFM::GetMFMEncoder(segment.data);
		bool did_sync = false;
		while(track_offset < 6250) {
			uint8_t next_byte = file_.get8();
			track_offset++;

			switch(next_byte) {
				default: {
					encoder->add_byte(next_byte);
					if(did_sync) {
						switch(next_byte) {
							default: break;

							case 0xfe:
								for(int byte = 0; byte < 6; byte++) {
									last_header[byte] = file_.get8();
									encoder->add_byte(last_header[byte]);
									++track_offset;
									if(track_offset == 6250) break;
								}
							break;

							case 0xfb:
								for(int byte = 0; byte < (128 << last_header[3]) + 2; byte++) {
									encoder->add_byte(file_.get8());
									++track_offset;
									// Special exception: don't interrupt a sector body if it seems to
									// be about to run over the end of the track. It seems like BD-500
									// disks break the usual 6250-byte rule, pushing out to just less
									// than 6400 bytes total.
									if(track_offset == 6400) break;
								}
							break;
						}
					}

					did_sync = false;
				}
				break;

				case 0xa1:	// a synchronisation mark that implies a sector or header coming
					encoder->output_short(Storage::Encodings::MFM::MFMSync);
					did_sync = true;
				break;

				case 0xc2:	// an 'ordinary' synchronisation mark
					encoder->output_short(Storage::Encodings::MFM::MFMIndexSync);
				break;
			}
		}
	}

	return std::make_shared<PCMTrack>(segment);
}

void OricMFMDSK::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	for(const auto &track : tracks) {
		PCMSegment segment = Storage::Disk::track_serialisation(*track.second, Storage::Encodings::MFM::MFMBitLength);
		Storage::Encodings::MFM::Shifter shifter;
		shifter.set_is_double_density(true);
		shifter.set_should_obey_syncs(true);
		std::vector<uint8_t> parsed_track;
		int size = 0;
		int offset = 0;
		bool capture_size = false;

		for(const auto bit : segment.data) {
			shifter.add_input_bit(bit ? 1 : 0);
			if(shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::None) continue;
			parsed_track.push_back(shifter.get_byte());

			if(offset) {
				offset--;
				if(!offset) {
					shifter.set_should_obey_syncs(true);
				}
				if(capture_size && offset == 2) {
					size = parsed_track.back();
					capture_size = false;
				}
			}

			if(	shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::Data ||
				shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::DeletedData) {
				offset = 128 << size;
				shifter.set_should_obey_syncs(false);
			}

			if(shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::ID) {
				offset = 6;
				shifter.set_should_obey_syncs(false);
				capture_size = true;
			}
		}

		long file_offset = get_file_offset_for_position(track.first);

		std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
		file_.seek(file_offset, SEEK_SET);
		std::size_t track_size = std::min(size_t(6400), parsed_track.size());
		file_.write(parsed_track.data(), track_size);
	}
}

bool OricMFMDSK::get_is_read_only() {
	return file_.get_is_known_read_only();
}
