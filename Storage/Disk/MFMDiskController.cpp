//
//  MFMDiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MFMDiskController.hpp"

#include "../../Storage/Disk/Encodings/MFM.hpp"

using namespace Storage::Disk;

MFMController::MFMController(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute) :
	Storage::Disk::Controller(clock_rate, clock_rate_multiplier, revolutions_per_minute),
	crc_generator_(0x1021, 0xffff),
	data_mode_(DataMode::Scanning),
	is_awaiting_marker_value_(false) {
}

void MFMController::process_index_hole() {
	posit_event((int)Event::IndexHole);
}

void MFMController::process_write_completed() {
	posit_event((int)Event::DataWritten);
}

void MFMController::set_is_double_density(bool is_double_density) {
	is_double_density_ = is_double_density;
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_double_density ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	if(!is_double_density) is_awaiting_marker_value_ = false;
}

bool MFMController::get_is_double_density() {
	return is_double_density_;
}

void MFMController::set_data_mode(DataMode mode) {
	data_mode_ = mode;
}

MFMController::Token MFMController::get_latest_token() {
	return latest_token_;
}

NumberTheory::CRC16 &MFMController::get_crc_generator() {
	return crc_generator_;
}

void MFMController::process_input_bit(int value, unsigned int cycles_since_index_hole) {
	if(data_mode_ == DataMode::Writing) return;

	shift_register_ = (shift_register_ << 1) | value;
	bits_since_token_++;

	if(data_mode_ == DataMode::Scanning) {
		Token::Type token_type = Token::Byte;
		if(!is_double_density_) {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::FMIndexAddressMark:
					token_type = Token::Index;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IndexAddressByte);
				break;
				case Storage::Encodings::MFM::FMIDAddressMark:
					token_type = Token::ID;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IDAddressByte);
				break;
				case Storage::Encodings::MFM::FMDataAddressMark:
					token_type = Token::Data;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DataAddressByte);
				break;
				case Storage::Encodings::MFM::FMDeletedDataAddressMark:
					token_type = Token::DeletedData;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DeletedDataAddressByte);
				break;
				default:
				break;
			}
		} else {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::MFMIndexSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMIndexSyncByteValue;
				break;
				case Storage::Encodings::MFM::MFMSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
					crc_generator_.set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMSyncByteValue;
				break;
				default:
				break;
			}
		}

		if(token_type != Token::Byte) {
			latest_token_.type = token_type;
			bits_since_token_ = 0;
			posit_event((int)Event::Token);
			return;
		}
	}

	if(bits_since_token_ == 16) {
		latest_token_.type = Token::Byte;
		latest_token_.byte_value = (uint8_t)(
			((shift_register_ & 0x0001) >> 0) |
			((shift_register_ & 0x0004) >> 1) |
			((shift_register_ & 0x0010) >> 2) |
			((shift_register_ & 0x0040) >> 3) |
			((shift_register_ & 0x0100) >> 4) |
			((shift_register_ & 0x0400) >> 5) |
			((shift_register_ & 0x1000) >> 6) |
			((shift_register_ & 0x4000) >> 7));
		bits_since_token_ = 0;

		if(is_awaiting_marker_value_ && is_double_density_) {
			is_awaiting_marker_value_ = false;
			switch(latest_token_.byte_value) {
				case Storage::Encodings::MFM::IndexAddressByte:
					latest_token_.type = Token::Index;
				break;
				case Storage::Encodings::MFM::IDAddressByte:
					latest_token_.type = Token::ID;
				break;
				case Storage::Encodings::MFM::DataAddressByte:
					latest_token_.type = Token::Data;
				break;
				case Storage::Encodings::MFM::DeletedDataAddressByte:
					latest_token_.type = Token::DeletedData;
				break;
				default: break;
			}
		}

		crc_generator_.add(latest_token_.byte_value);
		posit_event((int)Event::Token);
		return;
	}
}

void MFMController::write_bit(int bit) {
	if(is_double_density_) {
		Controller::write_bit(!bit && !last_bit_);
		Controller::write_bit(!!bit);
		last_bit_ = bit;
	} else {
		Controller::write_bit(true);
		Controller::write_bit(!!bit);
	}
}

void MFMController::write_byte(uint8_t byte) {
	for(int c = 0; c < 8; c++) write_bit((byte << c)&0x80);
	crc_generator_.add(byte);
}

void MFMController::write_raw_short(uint16_t value) {
	for(int c = 0; c < 16; c++) {
		Controller::write_bit(!!((value << c)&0x8000));
	}
}
