//
//  TapeUEF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapeUEF.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#pragma mark - ZLib extensions

static float gzgetfloat(gzFile file) {
	uint8_t bytes[4];
	gzread(file, bytes, 4);

	/* assume a four byte array named Float exists, where Float[0]
	was the first byte read from the UEF, Float[1] the second, etc */

	/* decode mantissa */
	int mantissa;
	mantissa = bytes[0] | (bytes[1] << 8) | ((bytes[2]&0x7f)|0x80) << 16;

	float result = (float)mantissa;
	result = (float)ldexp(result, -23);

	/* decode exponent */
	int exponent;
	exponent = ((bytes[2]&0x80) >> 7) | (bytes[3]&0x7f) << 1;
	exponent -= 127;
	result = (float)ldexp(result, exponent);

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

UEF::UEF(const char *file_name) :
	time_base_(1200),
	is_300_baud_(false) {
	file_ = gzopen(file_name, "rb");

	char identifier[10];
	int bytes_read = gzread(file_, identifier, 10);
	if(bytes_read < 10 || strcmp(identifier, "UEF File!")) {
		throw ErrorNotUEF;
	}

	uint8_t version[2];
	gzread(file_, version, 2);

	if(version[1] > 0 || version[0] > 10) {
		throw ErrorNotUEF;
	}
}

UEF::~UEF()
{
	gzclose(file_);
}

#pragma mark - Public methods

void UEF::virtual_reset() {
	gzseek(file_, 12, SEEK_SET);
	set_is_at_end(false);
	clear();
}

#pragma mark - Chunk navigator

void UEF::get_next_pulses() {
	while(empty()) {
		// read chunk details
		uint16_t chunk_id = (uint16_t)gzget16(file_);
		uint32_t chunk_length = (uint32_t)gzget32(file_);

		// figure out where the next chunk will start
		z_off_t start_of_next_chunk = gztell(file_) + chunk_length;

		if(gzeof(file_)) {
			set_is_at_end(true);
			return;
		}

		switch(chunk_id) {
			case 0x0100:	queue_implicit_bit_pattern(chunk_length);	break;
			case 0x0102:	queue_explicit_bit_pattern(chunk_length);	break;
			case 0x0112:	queue_integer_gap();						break;
			case 0x0116:	queue_floating_point_gap();					break;

			case 0x0110:	queue_carrier_tone();						break;
			case 0x0111:	queue_carrier_tone_with_dummy();			break;

			case 0x0114:	queue_security_cycles();					break;
			case 0x0104:	queue_defined_data(chunk_length);			break;

			// change of base rate
			case 0x0113: {
				// TODO: something smarter than just converting this to an int
				float new_time_base = gzgetfloat(file_);
				time_base_ = (unsigned int)roundf(new_time_base);
			}
			break;

			case 0x0117: {
				int baud_rate = gzget16(file_);
				is_300_baud_ = (baud_rate == 300);
			}
			break;

			default:
				printf("!!! Skipping %04x\n", chunk_id);
			break;
		}

		gzseek(file_, start_of_next_chunk, SEEK_SET);
	}
}

#pragma mark - Chunk parsers

void UEF::queue_implicit_bit_pattern(uint32_t length) {
	while(length--)
	{
		queue_implicit_byte(gzget8(file_));
	}
}

void UEF::queue_explicit_bit_pattern(uint32_t length) {
	size_t length_in_bits = (length << 3) - (size_t)gzget8(file_);
	uint8_t current_byte = 0;
	for(size_t bit = 0; bit < length_in_bits; bit++)
	{
		if(!(bit&7)) current_byte = gzget8(file_);
		queue_bit(current_byte&1);
		current_byte >>= 1;
	}
}

void UEF::queue_integer_gap() {
	Time duration;
	duration.length = (unsigned int)gzget16(file_);
	duration.clock_rate = time_base_;
	emplace_back(Pulse::Zero, duration);
}

void UEF::queue_floating_point_gap() {
	float length = gzgetfloat(file_);
	Time duration;
	duration.length = (unsigned int)(length * 4000000);
	duration.clock_rate = 4000000;
	emplace_back(Pulse::Zero, duration);
}

void UEF::queue_carrier_tone() {
	unsigned int number_of_cycles = (unsigned int)gzget16(file_);
	while(number_of_cycles--) queue_bit(1);
}

void UEF::queue_carrier_tone_with_dummy() {
	unsigned int pre_cycles = (unsigned int)gzget16(file_);
	unsigned int post_cycles = (unsigned int)gzget16(file_);
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

	int bits_per_packet = gzget8(file_);
	char parity_type = (char)gzget8(file_);
	int number_of_stop_bits = gzget8(file_);

	bool has_extra_stop_wave = (number_of_stop_bits < 0);
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

#pragma mark - Queuing helpers

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
