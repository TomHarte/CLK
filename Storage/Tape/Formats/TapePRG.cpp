//
//  TapePRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TapePRG.hpp"

/*
	My interpretation of Commodore's tape format is such that a PRG is encoded as:

	[long block of lead-in tone]
	[short block of lead-in tone]
	[count down][header; 192 bytes fixed length]
	[short block of lead-in tone]
	[count down][copy of header; 192 bytes fixed length]
	[gap]
	[short block of lead-in tone]
	[count down][data; length as in file]
	[short block of lead-in tone]
	[count down][copy of data]
	... and repeat ...

	Individual bytes are composed of:

		word marker
		least significant bit
		...
		most significant bit
		parity bit

	Both the header and data blocks additionally end with an end-of-block marker.

	Encoding is via square-wave cycles of four lengths, in ascending order: lead-in, zero, one, marker.

	Lead-in tone is always just repetitions of the lead-in wave.
	A word marker is a marker wave followed by a one wave.
	An end-of-block marker is a marker wave followed by a zero wave.
	A zero bit is a zero wave followed by a one wave.
	A one bit is a one wave followed by a zero wave.

	Parity is 1 if there are an even number of bits in the byte; 0 otherwise.
*/

#include <sys/stat.h>

using namespace Storage::Tape;

PRG::PRG(const std::string &file_name) :
	file_(file_name)
{
	// There's really no way to validate other than that if this file is larger than 64kb,
	// of if load address + length > 65536 then it's broken.
	if(file_.stats().st_size >= 65538 || file_.stats().st_size < 3)
		throw ErrorBadFormat;

	load_address_ = file_.get16le();
	length_ = uint16_t(file_.stats().st_size - 2);

	if (load_address_ + length_ >= 65536)
		throw ErrorBadFormat;
}

Storage::Tape::Tape::Pulse PRG::virtual_get_next_pulse() {
	// these are all microseconds per pole
	constexpr unsigned int leader_zero_length = 179;
	constexpr unsigned int zero_length = 169;
	constexpr unsigned int one_length = 247;
	constexpr unsigned int marker_length = 328;

	bit_phase_ = (bit_phase_+1)&3;
	if(!bit_phase_) get_next_output_token();

	Tape::Pulse pulse;
	pulse.length.clock_rate = 1000000;
	pulse.type = (bit_phase_&1) ? Tape::Pulse::High : Tape::Pulse::Low;
	switch(output_token_) {
		case Leader:		pulse.length.length = leader_zero_length;							break;
		case Zero:			pulse.length.length = (bit_phase_&2) ? one_length : zero_length;		break;
		case One:			pulse.length.length = (bit_phase_&2) ? zero_length : one_length;		break;
		case WordMarker:	pulse.length.length = (bit_phase_&2) ? one_length : marker_length;	break;
		case EndOfBlock:	pulse.length.length = (bit_phase_&2) ? zero_length : marker_length;	break;
		case Silence:		pulse.type = Tape::Pulse::Zero; pulse.length.length = 5000;			break;
	}
	return pulse;
}

void PRG::virtual_reset() {
	bit_phase_ = 3;
	file_.seek(2, SEEK_SET);
	file_phase_ = FilePhaseLeadIn;
	phase_offset_ = 0;
	copy_mask_ = 0x80;
}

bool PRG::is_at_end() {
	return file_phase_ == FilePhaseAtEnd;
}

void PRG::get_next_output_token() {
	constexpr int block_length = 192;	// not counting the checksum
	constexpr int countdown_bytes = 9;
	constexpr int leadin_length = 20000;
	constexpr int block_leadin_length = 5000;

	if(file_phase_ == FilePhaseHeaderDataGap || file_phase_ == FilePhaseAtEnd) {
		output_token_ = Silence;
		if(file_phase_ != FilePhaseAtEnd) file_phase_ = FilePhaseData;
		return;
	}

	// the lead-in is 20,000 instances of the lead-in pair; every other phase begins with 5000
	// before doing whatever it should be doing
	if(file_phase_ == FilePhaseLeadIn || phase_offset_ < block_leadin_length) {
		output_token_ = Leader;
		phase_offset_++;
		if(file_phase_ == FilePhaseLeadIn && phase_offset_ == leadin_length) {
			phase_offset_ = 0;
			file_phase_ = (file_phase_ == FilePhaseLeadIn) ? FilePhaseHeader : FilePhaseData;
		}
		return;
	}

	// determine whether a new byte needs to be queued up
	int block_offset = phase_offset_ - block_leadin_length;
	int bit_offset = block_offset % 10;
	int byte_offset = block_offset / 10;
	phase_offset_++;

	if(!bit_offset &&
		(
			(file_phase_ == FilePhaseHeader && byte_offset == block_length + countdown_bytes + 1) ||
			file_.eof()
		)
	) {
		output_token_ = EndOfBlock;
		phase_offset_ = 0;

		switch(file_phase_) {
			default: break;
			case FilePhaseHeader:
				copy_mask_ ^= 0x80;
				if(copy_mask_) file_phase_ = FilePhaseHeaderDataGap;
			break;
			case FilePhaseData:
				copy_mask_ ^= 0x80;
				file_.seek(2, SEEK_SET);
				if(copy_mask_) file_phase_ = FilePhaseAtEnd;
			break;
		}
		return;
	}

	if(bit_offset == 0) {
		// the first nine bytes are countdown; the high bit is set if this is a header
		if(byte_offset < countdown_bytes) {
			output_byte_ = uint8_t(countdown_bytes - byte_offset) | copy_mask_;
		} else {
			if(file_phase_ == FilePhaseHeader) {
				if(byte_offset == countdown_bytes + block_length) {
					output_byte_ = check_digit_;
				} else {
					if(byte_offset == countdown_bytes) check_digit_ = 0;
					if(file_phase_ == FilePhaseHeader) {
						switch(byte_offset - countdown_bytes) {
							case 0:	output_byte_ = 0x03;										break;
							case 1: output_byte_ = load_address_ & 0xff;						break;
							case 2: output_byte_ = (load_address_ >> 8)&0xff;					break;
							case 3: output_byte_ = (load_address_ + length_) & 0xff;			break;
							case 4: output_byte_ = ((load_address_ + length_) >> 8) & 0xff;		break;

							case 5: output_byte_ = 0x50;	break; // P
							case 6: output_byte_ = 0x52;	break; // R
							case 7: output_byte_ = 0x47;	break; // G
							default:
								output_byte_ = 0x20;
							break;
						}
					}
				}
			} else {
				output_byte_ = file_.get8();
				if(file_.eof()) {
					output_byte_ = check_digit_;
				}
			}

			check_digit_ ^= output_byte_;
		}
	}

	switch(bit_offset) {
		case 0:
			output_token_ = WordMarker;
		break;
		default:	// i.e. 1-8
			output_token_ = (output_byte_ & (1 << (bit_offset - 1))) ? One : Zero;
		break;
		case 9: {
			uint8_t parity = output_byte_;
			parity ^= (parity >> 4);
			parity ^= (parity >> 2);
			parity ^= (parity >> 1);
			output_token_ = (parity&1) ? Zero : One;
		}
		break;
	}
}
