//
//  TapeUEF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TapeUEF.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#define LOG_PREFIX "[UEF] "
#include "../../../Outputs/Log.hpp"

// MARK: - ZLib extensions

static float gzgetfloat(gzFile file) {
	uint8_t bytes[4];
	gzread(file, bytes, 4);

	/* assume a four byte array named Float exists, where Float[0]
	was the first byte read from the UEF, Float[1] the second, etc */

	/* decode mantissa */
	int mantissa;
	mantissa = bytes[0] | (bytes[1] << 8) | ((bytes[2]&0x7f)|0x80) << 16;

	float result = float(mantissa);
	result = float(ldexp(result, -23));

	/* decode exponent */
	int exponent;
	exponent = ((bytes[2]&0x80) >> 7) | (bytes[3]&0x7f) << 1;
	exponent -= 127;
	result = float(ldexp(result, exponent));

	/* flip sign if necessary */
	if(bytes[3]&0x80)
		result = -result;

	return result;
}

static uint8_t gzget8(gzFile file) {
	// This is a workaround for gzgetc, which seems to be broken in ZLib 1.2.8.
	uint8_t result;
	gzread(file, &result, 1);
	return result;
}

static int gzget16(gzFile file) {
	uint8_t bytes[2];
	gzread(file, bytes, 2);
	return bytes[0] | (bytes[1] << 8);
}

static int gzget24(gzFile file) {
	uint8_t bytes[3];
	gzread(file, bytes, 3);
	return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
}

static int gzget32(gzFile file) {
	uint8_t bytes[4];
	gzread(file, bytes, 4);
	return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

using namespace Storage::Tape;

UEF::UEF(const std::string &file_name) {
	file_ = gzopen(file_name.c_str(), "rb");

	char identifier[10];
	int bytes_read = gzread(file_, identifier, 10);
	if(bytes_read < 10 || std::strcmp(identifier, "UEF File!")) {
		throw ErrorNotUEF;
	}

	uint8_t version[2];
	gzread(file_, version, 2);

	if(version[1] > 0 || version[0] > 10) {
		throw ErrorNotUEF;
	}

	set_platform_type();
}

UEF::~UEF() {
	gzclose(file_);
}

// MARK: - Public methods

void UEF::virtual_reset() {
	gzseek(file_, 12, SEEK_SET);
	set_is_at_end(false);
	clear();
}

// MARK: - Chunk navigator

bool UEF::get_next_chunk(UEF::Chunk &result) {
	const uint16_t chunk_id = uint16_t(gzget16(file_));
	const uint32_t chunk_length = uint32_t(gzget32(file_));
	const z_off_t start_of_next_chunk = gztell(file_) + chunk_length;

	if(gzeof(file_)) {
		return false;
	}

	result.id = chunk_id;
	result.length = chunk_length;
	result.start_of_next_chunk = start_of_next_chunk;

	return true;
}

void UEF::get_next_pulses() {
	while(empty()) {
		// read chunk details
		Chunk next_chunk;
		if(!get_next_chunk(next_chunk)) {
			set_is_at_end(true);
			return;
		}

		switch(next_chunk.id) {
			case 0x0100:	queue_implicit_bit_pattern(next_chunk.length);	break;
			case 0x0102:	queue_explicit_bit_pattern(next_chunk.length);	break;
			case 0x0112:	queue_integer_gap();							break;
			case 0x0116:	queue_floating_point_gap();						break;

			case 0x0110:	queue_carrier_tone();							break;
			case 0x0111:	queue_carrier_tone_with_dummy();				break;

			case 0x0114:	queue_security_cycles();						break;
			case 0x0104:	queue_defined_data(next_chunk.length);			break;

			// change of base rate
			case 0x0113: {
				// TODO: something smarter than just converting this to an int
				const float new_time_base = gzgetfloat(file_);
				time_base_ = unsigned(roundf(new_time_base));
			}
			break;

			case 0x0117: {
				const int baud_rate = gzget16(file_);
				is_300_baud_ = (baud_rate == 300);
			}
			break;

			default:
				LOG("Skipping chunk of type " << PADHEX(4) << next_chunk.id);
			break;
		}

		gzseek(file_, next_chunk.start_of_next_chunk, SEEK_SET);
	}
}

// MARK: - Chunk parsers

void UEF::queue_implicit_bit_pattern(uint32_t length) {
	while(length--) {
		queue_implicit_byte(gzget8(file_));
	}
}

void UEF::queue_explicit_bit_pattern(uint32_t length) {
	const std::size_t length_in_bits = (length << 3) - size_t(gzget8(file_));
	uint8_t current_byte = 0;
	for(std::size_t bit = 0; bit < length_in_bits; bit++) {
		if(!(bit&7)) current_byte = gzget8(file_);
		queue_bit(current_byte&1);
		current_byte >>= 1;
	}
}

void UEF::queue_integer_gap() {
	Time duration;
	duration.length = unsigned(gzget16(file_));
	duration.clock_rate = time_base_;
	emplace_back(Pulse::Zero, duration);
}

void UEF::queue_floating_point_gap() {
	const float length = gzgetfloat(file_);
	Time duration;
	duration.length = unsigned(length * 4000000);
	duration.clock_rate = 4000000;
	emplace_back(Pulse::Zero, duration);
}

void UEF::queue_carrier_tone() {
	unsigned int number_of_cycles = unsigned(gzget16(file_));
	while(number_of_cycles--) queue_bit(1);
}

void UEF::queue_carrier_tone_with_dummy() {
	unsigned int pre_cycles = unsigned(gzget16(file_));
	unsigned int post_cycles = unsigned(gzget16(file_));
	while(pre_cycles--) queue_bit(1);
	queue_implicit_byte(0xaa);
	while(post_cycles--) queue_bit(1);
}

void UEF::queue_security_cycles() {
	int number_of_cycles = gzget24(file_);
	bool first_is_pulse = gzget8(file_) == 'P';
	bool last_is_pulse = gzget8(file_) == 'P';

	uint8_t current_byte = 0;
	for(int cycle = 0; cycle < number_of_cycles; cycle++) {
		if(!(cycle&7)) current_byte = gzget8(file_);
		int bit = (current_byte >> 7);
		current_byte <<= 1;

		Time duration;
		duration.length = bit ? 1 : 2;
		duration.clock_rate = time_base_ * 4;

		if(!cycle && first_is_pulse) {
			emplace_back(Pulse::High, duration);
		} else if(cycle == number_of_cycles-1 && last_is_pulse) {
			emplace_back(Pulse::Low, duration);
		} else {
			emplace_back(Pulse::Low, duration);
			emplace_back(Pulse::High, duration);
		}
	}
}

void UEF::queue_defined_data(uint32_t length) {
	if(length < 3) return;

	const int bits_per_packet = gzget8(file_);
	const char parity_type = char(gzget8(file_));
	int number_of_stop_bits = gzget8(file_);

	const bool has_extra_stop_wave = (number_of_stop_bits < 0);
	number_of_stop_bits = abs(number_of_stop_bits);

	length -= 3;
	while(length--) {
		uint8_t byte = gzget8(file_);

		uint8_t parity_value = byte;
		parity_value ^= (parity_value >> 4);
		parity_value ^= (parity_value >> 2);
		parity_value ^= (parity_value >> 1);

		queue_bit(0);
		int c = bits_per_packet;
		while(c--) {
			queue_bit(byte&1);
			byte >>= 1;
		}

		switch(parity_type) {
			default: break;
			case 'E': queue_bit(parity_value&1);		break;
			case 'O': queue_bit((parity_value&1) ^ 1);	break;
		}

		int stop_bits = number_of_stop_bits;
		while(stop_bits--) queue_bit(1);
		if(has_extra_stop_wave) {
			Time duration;
			duration.length = 1;
			duration.clock_rate = time_base_ * 4;
			emplace_back(Pulse::Low, duration);
			emplace_back(Pulse::High, duration);
		}
	}
}

// MARK: - Queuing helpers

void UEF::queue_implicit_byte(uint8_t byte) {
	queue_bit(0);
	int c = 8;
	while(c--) {
		queue_bit(byte&1);
		byte >>= 1;
	}
	queue_bit(1);
}

void UEF::queue_bit(int bit) {
	int number_of_cycles;
	Time duration;
	duration.clock_rate = time_base_ * 4;

	if(bit) {
		// encode high-frequency waves
		duration.length = 1;
		number_of_cycles = 2;
	} else {
		// encode low-frequency waves
		duration.length = 2;
		number_of_cycles = 1;
	}

	if(is_300_baud_) number_of_cycles *= 4;

	while(number_of_cycles--) {
		emplace_back(Pulse::Low, duration);
		emplace_back(Pulse::High, duration);
	}
}

// MARK: - TypeDistinguisher

TargetPlatform::Type UEF::target_platform_type() {
	return platform_type_;
}

void UEF::set_platform_type() {
	// If a chunk of type 0005 exists anywhere in the UEF then the UEF specifies its target machine.
	// So check and, if so, update the list of machines for which this file thinks it is suitable.
	Chunk next_chunk;
	while(get_next_chunk(next_chunk)) {
		if(next_chunk.id == 0x0005) {
			uint8_t target = gzget8(file_);
			switch(target >> 4) {
				case 0:	platform_type_ = TargetPlatform::BBCModelA;		break;
				case 1:	platform_type_ = TargetPlatform::AcornElectron;	break;
				case 2:	platform_type_ = TargetPlatform::BBCModelB;		break;
				case 3:	platform_type_ = TargetPlatform::BBCMaster;		break;
				case 4:	platform_type_ = TargetPlatform::AcornAtom;		break;
				default: break;
			}
		}
		gzseek(file_, next_chunk.start_of_next_chunk, SEEK_SET);
	}
	reset();
}
