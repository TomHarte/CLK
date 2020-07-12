//
//  MFMDiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MFMDiskController.hpp"

#include "../Encodings/MFM/Constants.hpp"

using namespace Storage::Disk;

MFMController::MFMController(Cycles clock_rate) :
	Storage::Disk::Controller(clock_rate),
	shifter_(&crc_generator_) {
}

void MFMController::process_index_hole() {
	posit_event(int(Event::IndexHole));
}

void MFMController::process_write_completed() {
	posit_event(int(Event::DataWritten));
}

void MFMController::set_is_double_density(bool is_double_density) {
	is_double_density_ = is_double_density;
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_double_density ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	shifter_.set_is_double_density(is_double_density);
}

bool MFMController::get_is_double_density() {
	return is_double_density_;
}

void MFMController::set_data_mode(DataMode mode) {
	data_mode_ = mode;
	shifter_.set_should_obey_syncs(mode == DataMode::Scanning);
}

MFMController::Token MFMController::get_latest_token() {
	return latest_token_;
}

CRC::CCITT &MFMController::get_crc_generator() {
	return crc_generator_;
}

void MFMController::process_input_bit(int value) {
	if(data_mode_ == DataMode::Writing) return;

	shifter_.add_input_bit(value);
	switch(shifter_.get_token()) {
		case Encodings::MFM::Shifter::Token::None:
		return;

		case Encodings::MFM::Shifter::Token::Index:
			latest_token_.type = Token::Index;
		break;
		case Encodings::MFM::Shifter::Token::ID:
			latest_token_.type = Token::ID;
		break;
		case Encodings::MFM::Shifter::Token::Data:
			latest_token_.type = Token::Data;
		break;
		case Encodings::MFM::Shifter::Token::DeletedData:
			latest_token_.type = Token::DeletedData;
		break;
		case Encodings::MFM::Shifter::Token::Sync:
			latest_token_.type = Token::Sync;
		break;
		case Encodings::MFM::Shifter::Token::Byte:
			latest_token_.type = Token::Byte;
		break;
	}
	latest_token_.byte_value = shifter_.get_byte();
	posit_event(int(Event::Token));
}

void MFMController::write_bit(int bit) {
	if(is_double_density_) {
		get_drive().write_bit(!bit && !last_bit_);
		get_drive().write_bit(!!bit);
		last_bit_ = bit;
	} else {
		get_drive().write_bit(true);
		get_drive().write_bit(!!bit);
	}
}

void MFMController::write_byte(uint8_t byte) {
	for(int c = 0; c < 8; c++) write_bit((byte << c)&0x80);
	crc_generator_.add(byte);
}

void MFMController::write_raw_short(uint16_t value) {
	for(int c = 0; c < 16; c++) {
		get_drive().write_bit(!!((value << c)&0x8000));
	}
}

void MFMController::write_crc() {
	uint16_t crc = get_crc_generator().get_value();
	write_byte(crc >> 8);
	write_byte(crc & 0xff);
}

void MFMController::write_n_bytes(int quantity, uint8_t value) {
	while(quantity--) write_byte(value);
}

void MFMController::write_id_joiner() {
	if(get_is_double_density()) {
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
		get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
		write_byte(Storage::Encodings::MFM::IDAddressByte);
	} else {
		write_n_bytes(6, 0x00);
		get_crc_generator().reset();
		write_raw_short(Storage::Encodings::MFM::FMIDAddressMark);
	}
}

void MFMController::write_id_data_joiner(bool is_deleted, bool skip_first_gap) {
	if(get_is_double_density()) {
		if(!skip_first_gap) write_n_bytes(22, 0x4e);
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
		get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
		write_byte(is_deleted ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
	} else {
		if(!skip_first_gap) write_n_bytes(11, 0xff);
		write_n_bytes(6, 0x00);
		get_crc_generator().reset();
		get_crc_generator().add(is_deleted ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
		write_raw_short(is_deleted ? Storage::Encodings::MFM::FMDeletedDataAddressMark : Storage::Encodings::MFM::FMDataAddressMark);
	}
}

void MFMController::write_post_data_gap() {
	if(get_is_double_density()) {
		write_n_bytes(54, 0x4e);
	} else {
		write_n_bytes(27, 0xff);
	}
}

void MFMController::write_start_of_track() {
	if(get_is_double_density()) {
		write_n_bytes(80, 0x4e);
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMIndexSync);
		write_byte(Storage::Encodings::MFM::IndexAddressByte);
		write_n_bytes(50, 0x4e);
	} else {
		write_n_bytes(40, 0xff);
		write_n_bytes(6, 0x00);
		write_raw_short(Storage::Encodings::MFM::FMIndexAddressMark);
		write_n_bytes(26, 0xff);
	}
}
