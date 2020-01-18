//
//  Shifter.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Shifter.hpp"
#include "Constants.hpp"

using namespace Storage::Encodings::MFM;

Shifter::Shifter() : owned_crc_generator_(new CRC::CCITT()), crc_generator_(owned_crc_generator_.get()) {}
Shifter::Shifter(CRC::CCITT *crc_generator) : crc_generator_(crc_generator) {}

void Shifter::set_is_double_density(bool is_double_density) {
	is_double_density_ = is_double_density;
	if(!is_double_density) is_awaiting_marker_value_ = false;
}

void Shifter::set_should_obey_syncs(bool should_obey_syncs) {
	should_obey_syncs_ = should_obey_syncs;
}

void Shifter::add_input_bit(int value) {
	shift_register_ = (shift_register_ << 1) | static_cast<unsigned int>(value);
	++bits_since_token_;

	token_ = Token::None;
	if(should_obey_syncs_) {
		if(!is_double_density_) {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::FMIndexAddressMark:
					token_ = Token::Index;
					crc_generator_->reset();
					crc_generator_->add(Storage::Encodings::MFM::IndexAddressByte);
				break;
				case Storage::Encodings::MFM::FMIDAddressMark:
					token_ = Token::ID;
					crc_generator_->reset();
					crc_generator_->add(Storage::Encodings::MFM::IDAddressByte);
				break;
				case Storage::Encodings::MFM::FMDataAddressMark:
					token_ = Token::Data;
					crc_generator_->reset();
					crc_generator_->add(Storage::Encodings::MFM::DataAddressByte);
				break;
				case Storage::Encodings::MFM::FMDeletedDataAddressMark:
					token_ = Token::DeletedData;
					crc_generator_->reset();
					crc_generator_->add(Storage::Encodings::MFM::DeletedDataAddressByte);
				break;
				default:
				break;
			}
		} else {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::MFMIndexSync:
					// This models a bit of slightly weird WD behaviour;
					// if an index sync was detected where it wasn't expected,
					// the WD will resync but also may return the clock bits
					// rather than data bits as the next byte read, depending
					// on framing.
					//
					// TODO: make this optional, if ever a Shifter with
					// well-defined non-WD behaviour is needed.
					//
					// TODO: Verify WD behaviour.
					if(bits_since_token_&1) {
						shift_register_ >>= 1;
					}

					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;

					token_ = Token::Sync;
				break;
				case Storage::Encodings::MFM::MFMSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
					crc_generator_->set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);

					token_ = Token::Sync;
				break;
				default:
				break;
			}
		}

		if(token_ != Token::None) {
			bits_since_token_ = 0;
			return;
		}
	}

	if(bits_since_token_ == 16) {
		token_ = Token::Byte;
		bits_since_token_ = 0;

		if(is_awaiting_marker_value_ && is_double_density_) {
			is_awaiting_marker_value_ = false;
			switch(get_byte()) {
				case Storage::Encodings::MFM::IndexAddressByte:
					token_ = Token::Index;
				break;
				case Storage::Encodings::MFM::IDAddressByte:
					token_ = Token::ID;
				break;
				case Storage::Encodings::MFM::DataAddressByte:
					token_ = Token::Data;
				break;
				case Storage::Encodings::MFM::DeletedDataAddressByte:
					token_ = Token::DeletedData;
				break;
				default: break;
			}
		}

		crc_generator_->add(get_byte());
	}
}

uint8_t Shifter::get_byte() const {
	return uint8_t(
		((shift_register_ & 0x0001) >> 0) |
		((shift_register_ & 0x0004) >> 1) |
		((shift_register_ & 0x0010) >> 2) |
		((shift_register_ & 0x0040) >> 3) |
		((shift_register_ & 0x0100) >> 4) |
		((shift_register_ & 0x0400) >> 5) |
		((shift_register_ & 0x1000) >> 6) |
		((shift_register_ & 0x4000) >> 7));
}
