//
//  Parser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "Parser.hpp"

#include "Constants.hpp"
#include "../../DiskImage/DiskImage.hpp"
#include "../../SingleTrackDisk/SingleTrackDisk.hpp"

using namespace Storage::Encodings::MFM;

Parser::Parser(bool is_mfm) :
		Storage::Disk::Controller(4000000),
		crc_generator_(0x1021, 0xffff),
		shift_register_(0), is_mfm_(is_mfm),
		track_(0), head_(0) {
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_mfm ? 500000 : 250000;	// i.e. 250 kbps (including clocks)
	set_expected_bit_length(bit_length);

	drive_.reset(new Storage::Disk::Drive(4000000, 300, 2));
	set_drive(drive_);
	drive_->set_motor_on(true);
}

Parser::Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Disk> &disk) :
		Parser(is_mfm) {
	drive_->set_disk(disk);
}

Parser::Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Track> &track) :
		Parser(is_mfm) {
	drive_->set_disk(std::make_shared<Disk::DiskImageHolder<Disk::SingleTrackDiskImage>>(track));
}

void Parser::seek_to_track(uint8_t track) {
	int difference = (int)track - (int)track_;
	track_ = track;

	if(difference) {
		int direction = difference < 0 ? -1 : 1;
		difference *= direction;

		for(int c = 0; c < difference; c++) drive_->step(direction);
	}
}

std::shared_ptr<Sector> Parser::get_sector(uint8_t head, uint8_t track, uint8_t sector) {
	// Switch head and track if necessary.
	if(head_ != head) {
		drive_->set_head(head);
	}
	seek_to_track(track);
	int track_index = get_index(head, track, 0);

	// Populate the sector cache if it's not already populated by asking for sectors unless and until
	// one is returned that has already been seen.
	if(decoded_tracks_.find(track_index) == decoded_tracks_.end()) {
		std::shared_ptr<Sector> first_sector = get_next_sector();
		std::set<uint8_t> visited_sectors;
		if(first_sector) {
			while(1) {
				std::shared_ptr<Sector> next_sector = get_next_sector();
				if(next_sector) {
					if(visited_sectors.find(next_sector->address.sector) != visited_sectors.end()) {
						break;
					}
					visited_sectors.insert(next_sector->address.sector);
				}
			}
		}
		decoded_tracks_.insert(track_index);
	}

	// Check cache for sector.
	int index = get_index(head, track, sector);
	auto cached_sector = sectors_by_index_.find(index);
	if(cached_sector != sectors_by_index_.end()) {
		return cached_sector->second;
	}

	// If it wasn't found, it doesn't exist.
	return nullptr;
}

std::vector<uint8_t> Parser::get_track(uint8_t track) {
	seek_to_track(track);
	return get_track();
}

void Parser::process_input_bit(int value) {
	shift_register_ = ((shift_register_ << 1) | (unsigned int)value) & 0xffff;
	bit_count_++;
}

void Parser::process_index_hole() {
	index_count_++;
}

uint8_t Parser::get_byte_for_shift_value(uint16_t value) {
	return (uint8_t)(
		((value&0x0001) >> 0) |
		((value&0x0004) >> 1) |
		((value&0x0010) >> 2) |
		((value&0x0040) >> 3) |
		((value&0x0100) >> 4) |
		((value&0x0400) >> 5) |
		((value&0x1000) >> 6) |
		((value&0x4000) >> 7));
}

uint8_t Parser::get_next_byte() {
	bit_count_ = 0;
	// Archetypal MFM is 500,000 bps given that the drive has an RPM of 300. Clock rate was
	// specified at 4,000,000. So that's an idealised 8 cycles per bit, Jump ahead 14
	// times that...
	run_for(Cycles(14 * 8));

	// ... and proceed at half-idealised-bit intervals to get the next bit. Then proceed very gingerly indeed.
	while(bit_count_ < 15) run_for(Cycles(4));
	while(bit_count_ < 16) run_for(Cycles(2));

	uint8_t byte = get_byte_for_shift_value((uint16_t)shift_register_);
	crc_generator_.add(byte);
	return byte;
}

std::vector<uint8_t> Parser::get_track() {
	std::vector<uint8_t> result;
	int distance_until_permissible_sync = 0;
	uint8_t last_id[6] = {0, 0, 0, 0, 0, 0};
	int last_id_pointer = 0;
	bool next_is_type = false;

	// align to the next index hole
	index_count_ = 0;
	while(!index_count_) run_for(Cycles(1));

	// capture every other bit until the next index hole
	index_count_ = 0;
	while(1) {
		// wait until either another bit or the index hole arrives
		bit_count_ = 0;
		bool found_sync = false;
		while(!index_count_ && !found_sync && bit_count_ < 16) {
			int previous_bit_count = bit_count_;
			run_for(Cycles(1));

			if(!distance_until_permissible_sync && bit_count_ != previous_bit_count) {
				uint16_t low_shift_register = (shift_register_&0xffff);
				if(is_mfm_) {
					found_sync = (low_shift_register == MFMIndexSync) || (low_shift_register == MFMSync);
				} else {
					found_sync =
						(low_shift_register == FMIndexAddressMark) ||
						(low_shift_register == FMIDAddressMark) ||
						(low_shift_register == FMDataAddressMark) ||
						(low_shift_register == FMDeletedDataAddressMark);
				}
			}
		}

		// if that was the index hole then finish
		if(index_count_) {
			if(bit_count_) result.push_back(get_byte_for_shift_value((uint16_t)(shift_register_ << (16 - bit_count_))));
			break;
		}

		// store whatever the current byte is
		uint8_t byte_value = get_byte_for_shift_value((uint16_t)shift_register_);
		result.push_back(byte_value);
		if(last_id_pointer < 6) last_id[last_id_pointer++] = byte_value;

		// if no syncs are permissible here, decrement the waiting period and perform no further contemplation
		bool found_id = false, found_data = false;
		if(distance_until_permissible_sync) {
			distance_until_permissible_sync--;
		} else {
			if(found_sync) {
				if(is_mfm_) {
					next_is_type = true;
				} else {
					switch(shift_register_&0xffff) {
						case FMIDAddressMark:			found_id = true;	break;
						case FMDataAddressMark:
						case FMDeletedDataAddressMark:	found_data = true;	break;
					}
				}
			} else if(next_is_type) {
				switch(byte_value) {
					case IDAddressByte:				found_id = true;	break;
					case DataAddressByte:
					case DeletedDataAddressByte:	found_data = true;	break;
				}
			}
		}

		if(found_id) {
			distance_until_permissible_sync = 6;
			last_id_pointer = 0;
		}

		if(found_data) {
			distance_until_permissible_sync = 128 << last_id[3];
		}
	}

	return result;
}

std::shared_ptr<Sector> Parser::get_next_sector() {
	std::shared_ptr<Sector> sector(new Sector);
	index_count_ = 0;

	while(index_count_ < 2) {
		// look for an ID address mark
		bool id_found = false;
		while(!id_found) {
			run_for(Cycles(1));
			if(is_mfm_) {
				while(shift_register_ == MFMSync) {
					uint8_t mark = get_next_byte();
					if(mark == IDAddressByte) {
						crc_generator_.set_value(MFMPostSyncCRCValue);
						id_found = true;
						break;
					}
				}
			} else {
				if(shift_register_ == FMIDAddressMark) {
					crc_generator_.reset();
					id_found = true;
				}
			}
			if(index_count_ >= 2) return nullptr;
		}

		crc_generator_.add(IDAddressByte);
		sector->address.track = get_next_byte();
		sector->address.side = get_next_byte();
		sector->address.sector = get_next_byte();
		sector->size = get_next_byte();
		uint16_t header_crc = crc_generator_.get_value();
		if((header_crc >> 8) != get_next_byte()) sector->has_header_crc_error = true;
		if((header_crc & 0xff) != get_next_byte()) sector->has_header_crc_error = true;

		// look for data mark
		bool data_found = false;
		while(!data_found) {
			run_for(Cycles(1));
			if(is_mfm_) {
				while(shift_register_ == MFMSync) {
					uint8_t mark = get_next_byte();
					if(mark == DataAddressByte) {
						crc_generator_.set_value(MFMPostSyncCRCValue);
						data_found = true;
						break;
					}
					if(mark == IDAddressByte) return nullptr;
				}
			} else {
				if(shift_register_ == FMDataAddressMark) {
					crc_generator_.reset();
					data_found = true;
				}
				if(shift_register_ == FMIDAddressMark) return nullptr;
			}
			if(index_count_ >= 2) return nullptr;
		}
		crc_generator_.add(DataAddressByte);

		size_t data_size = (size_t)(128 << sector->size);
		sector->data.reserve(data_size);
		for(size_t c = 0; c < data_size; c++) {
			sector->data.push_back(get_next_byte());
		}
		uint16_t data_crc = crc_generator_.get_value();
		if((data_crc >> 8) != get_next_byte()) sector->has_data_crc_error = true;
		if((data_crc & 0xff) != get_next_byte()) sector->has_data_crc_error = true;

		// Put this sector into the cache.
		int index = get_index(head_, track_, sector->address.sector);
		sectors_by_index_[index] = sector;

		return sector;
	}

	return nullptr;
}

std::shared_ptr<Sector> Parser::get_sector(uint8_t sector) {
	std::shared_ptr<Sector> first_sector;
	index_count_ = 0;
	while(!first_sector && index_count_ < 2) first_sector = get_next_sector();
	if(!first_sector) return nullptr;
	if(first_sector->address.sector == sector) return first_sector;

	while(1) {
		std::shared_ptr<Sector> next_sector = get_next_sector();
		if(!next_sector) continue;
		if(next_sector->address.sector == first_sector->address.sector) return nullptr;
		if(next_sector->address.sector == sector) return next_sector;
	}
}

int Parser::get_index(uint8_t head, uint8_t track, uint8_t sector) {
	return head | (track << 8) | (sector << 16);
}
