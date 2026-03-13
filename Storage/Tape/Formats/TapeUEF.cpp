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

#include "Outputs/Log.hpp"

namespace {
using Logger = Log::Logger<Log::Source::TapeUEF>;
}

using namespace Storage::Tape;

UEF::Parser::Parser(const std::string_view file_name) {
	file_ = gzopen(std::string(file_name).c_str(), "rb");

	char identifier[10];
	const int bytes_read = gzread(file_, identifier, 10);
	if(bytes_read < 10 || std::strcmp(identifier, "UEF File!")) {
		throw Storage::Tape::UEF::ErrorNotUEF;
	}

	uint8_t version[2];
	gzread(file_, version, 2);

	if(version[1] > 0 || version[0] > 10) {
		throw Storage::Tape::UEF::ErrorNotUEF;
	}

	start_of_next_chunk_ = gztell(file_);
}

void UEF::Parser::reset() {
	start_of_next_chunk_ = 12;
}

UEF::Parser::~Parser() {
	gzclose(file_);
}

template<>
uint8_t UEF::Parser::read<uint8_t>() {
	// This is a workaround for gzgetc, which seems to be broken in ZLib 1.2.8.
	uint8_t result;
	gzread(file_, &result, 1);
	return result;
}

template<>
uint16_t UEF::Parser::read<uint16_t>() {
	uint8_t bytes[2];
	gzread(file_, bytes, 2);
	return uint16_t(bytes[0] | (bytes[1] << 8));
}

template<>
uint32_t UEF::Parser::read<uint32_t, 3>() {
	uint8_t bytes[3];
	gzread(file_, bytes, 3);
	return uint32_t(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16));
}

template<>
uint32_t UEF::Parser::read<uint32_t>() {
	uint8_t bytes[4];
	gzread(file_, bytes, 4);
	return uint32_t(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

std::optional<UEF::Parser::Chunk> UEF::Parser::next() {
	gzseek(file_, start_of_next_chunk_, SEEK_SET);

	const uint16_t chunk_id = read<uint16_t>();
	const uint32_t chunk_length = read<uint32_t>();
	const auto start = gztell(file_);
	start_of_next_chunk_ = start + chunk_length;

	if(gzeof(file_)) {
		return std::nullopt;
	}

	return Chunk{
		.id = chunk_id,
		.length = chunk_length,
	};
}

template<>
float UEF::Parser::read<float>() {
	uint8_t bytes[4];
	gzread(file_, bytes, 4);

	/* assume a four byte array named Float exists, where Float[0]
	was the first byte read from the UEF, Float[1] the second, etc */

	/* decode mantissa */
	const int mantissa = bytes[0] | (bytes[1] << 8) | ((bytes[2]&0x7f)|0x80) << 16;

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

UEF::UEF(const std::string_view file_name) : file_name_(file_name) {
	Parser parser(file_name);

	// If a chunk of type 0005 exists anywhere in the UEF then the UEF specifies its target machine.
	// So check and, if so, update the list of machines for which this file thinks it is suitable.
	while(const auto chunk = parser.next()) {
		if(chunk->id == 0x0005) {
			const uint8_t target = parser.read<uint8_t>();
			switch(target >> 4) {
				case 0:	target_platforms_ = TargetPlatform::BBCModelA;		break;
				case 1:	target_platforms_ = TargetPlatform::AcornElectron;	break;
				case 2:	target_platforms_ = TargetPlatform::BBCModelB;		break;
				case 3:	target_platforms_ = TargetPlatform::BBCMaster;		break;
				case 4:	target_platforms_ = TargetPlatform::AcornAtom;		break;
				default: break;
			}
		}
	}
}

std::unique_ptr<FormatSerialiser> UEF::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

UEF::Serialiser::Serialiser(const std::string_view file_name): parser_(file_name) {
}

UEF::Serialiser::~Serialiser() {
}

// MARK: - Public methods

void UEF::Serialiser::reset() {
	parser_.reset();
	set_is_at_end(false);
	clear();
}

// MARK: - Chunk navigator

void UEF::Serialiser::push_next_pulses() {
	while(empty()) {
		// read chunk details
		const auto next_chunk = parser_.next();
		if(!next_chunk) {
			set_is_at_end(true);
			return;
		}

		switch(next_chunk->id) {
			case 0x0100:	queue_implicit_bit_pattern(next_chunk->length);	break;
			case 0x0102:	queue_explicit_bit_pattern(next_chunk->length);	break;
			case 0x0112:	queue_integer_gap();							break;
			case 0x0116:	queue_floating_point_gap();						break;

			case 0x0110:	queue_carrier_tone();							break;
			case 0x0111:	queue_carrier_tone_with_dummy();				break;

			case 0x0114:	queue_security_cycles();						break;
			case 0x0104:	queue_defined_data(next_chunk->length);			break;

			// change of base rate
			case 0x0113: {
				// TODO: something smarter than just converting this to an int
				const float new_time_base = parser_.read<float>();
				time_base_ = unsigned(roundf(new_time_base));
			}
			break;

			case 0x0117: {
				const auto baud_rate = parser_.read<uint16_t>();
				is_300_baud_ = (baud_rate == 300);
			}
			break;

			default:
				Logger::info().append("Skipping chunk of type %04x", next_chunk->id);
			break;
		}
	}
}

// MARK: - Chunk parsers

void UEF::Serialiser::queue_implicit_bit_pattern(uint32_t length) {
	while(length--) {
		queue_implicit_byte(parser_.read<uint8_t>());
	}
}

void UEF::Serialiser::queue_explicit_bit_pattern(const uint32_t length) {
	const std::size_t length_in_bits = (length << 3) - size_t(parser_.read<uint8_t>());
	uint8_t current_byte = 0;
	for(std::size_t bit = 0; bit < length_in_bits; bit++) {
		if(!(bit&7)) current_byte = parser_.read<uint8_t>();
		queue_bit(current_byte&1);
		current_byte >>= 1;
	}
}

void UEF::Serialiser::queue_integer_gap() {
	Time duration;
	duration.length = parser_.read<uint16_t>();
	duration.clock_rate = time_base_;
	emplace_back(Pulse::Zero, duration);
}

void UEF::Serialiser::queue_floating_point_gap() {
	const float length = parser_.read<float>();
	Time duration;
	duration.length = unsigned(length * 4000000);
	duration.clock_rate = 4000000;
	emplace_back(Pulse::Zero, duration);
}

void UEF::Serialiser::queue_carrier_tone() {
	auto number_of_cycles = parser_.read<uint16_t>();
	while(number_of_cycles--) queue_bit(1);
}

void UEF::Serialiser::queue_carrier_tone_with_dummy() {
	auto pre_cycles = parser_.read<uint16_t>();
	auto post_cycles = parser_.read<uint16_t>();
	while(pre_cycles--) queue_bit(1);
	queue_implicit_byte(0xaa);
	while(post_cycles--) queue_bit(1);
}

void UEF::Serialiser::queue_security_cycles() {
	auto number_of_cycles = parser_.read<uint32_t, 3>();
	bool first_is_pulse = parser_.read<uint8_t>() == 'P';
	bool last_is_pulse = parser_.read<uint8_t>() == 'P';

	uint8_t current_byte = 0;
	for(uint32_t cycle = 0; cycle < number_of_cycles; cycle++) {
		if(!(cycle&7)) current_byte = parser_.read<uint8_t>();
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

void UEF::Serialiser::queue_defined_data(uint32_t length) {
	if(length < 3) return;

	const int bits_per_packet = parser_.read<uint8_t>();
	const char parity_type = char(parser_.read<uint8_t>());
	int number_of_stop_bits = parser_.read<uint8_t>();

	const bool has_extra_stop_wave = (number_of_stop_bits < 0);
	number_of_stop_bits = abs(number_of_stop_bits);

	length -= 3;
	while(length--) {
		uint8_t byte = parser_.read<uint8_t>();

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

void UEF::Serialiser::queue_implicit_byte(uint8_t byte) {
	queue_bit(0);
	int c = 8;
	while(c--) {
		queue_bit(byte&1);
		byte >>= 1;
	}
	queue_bit(1);
}

void UEF::Serialiser::queue_bit(const int bit) {
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

// MARK: - TargetPlatform::Distinguisher

TargetPlatform::Type UEF::target_platforms() {
	return target_platforms_;
}
