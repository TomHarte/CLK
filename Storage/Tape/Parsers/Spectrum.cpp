//
//  Spectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Spectrum.hpp"

//
// Source used for the logic below was primarily https://sinclair.wiki.zxnet.co.uk/wiki/Spectrum_tape_interface
//

using namespace Storage::Tape::ZXSpectrum;

void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	if(pulse.type == Storage::Tape::Tape::Pulse::Type::Zero) {
		push_wave(WaveType::Gap);
		return;
	}

	// Only pulse duration matters; the ZX Spectrum et al do not rely on polarity.
	const float t_states = pulse.length.get<float>() * 3'500'000.0f;

	// Too long => gap.
	if(t_states > 2400.0f) {
		push_wave(WaveType::Gap);
		return;
	}

	// 1940–2400 t-states => pilot.
	if(t_states > 1940.0f) {
		push_wave(WaveType::Pilot);
		return;
	}

	// 1282–1940 t-states => one.
	if(t_states > 1282.0f) {
		push_wave(WaveType::One);
		return;
	}

	// 895–1282 => zero.
	if(t_states > 795.0f) {
		push_wave(WaveType::Zero);
		return;
	}

	// 701–895 => sync 2.
	if(t_states > 701.0f) {
		push_wave(WaveType::Sync2);
		return;
	}

	// Anything remaining above 600 => sync 1.
	if(t_states > 600.0f) {
		push_wave(WaveType::Sync1);
		return;
	}

	// Whatever this was, it's too short. Call it a gap.
	push_wave(WaveType::Gap);
}

void Parser::inspect_waves(const std::vector<Storage::Tape::ZXSpectrum::WaveType> &waves) {
	switch(waves[0]) {
		// Gap and Pilot map directly.
		case WaveType::Gap:		push_symbol(SymbolType::Gap, 1);	break;
		case WaveType::Pilot:	push_symbol(SymbolType::Pilot, 1);	break;

		// Encountering a sync 2 on its own is unexpected.
		case WaveType::Sync2:
			push_symbol(SymbolType::Gap, 1);
		break;

		// A sync 1 should be followed by a sync 2 in order to make a sync.
		case WaveType::Sync1:
			if(waves.size() < 2) return;
			if(waves[1] == WaveType::Sync2) {
				push_symbol(SymbolType::Sync, 2);
			} else {
				push_symbol(SymbolType::Gap, 1);
			}
		break;

		// Both one and zero waves should come in pairs.
		case WaveType::One:
		case WaveType::Zero:
			if(waves.size() < 2) return;
			if(waves[1] == waves[0]) {
				push_symbol(waves[0] == WaveType::One ? SymbolType::One : SymbolType::Zero, 2);
			} else {
				push_symbol(SymbolType::Gap, 1);
			}
		break;
	}
}

std::optional<Header> Parser::find_header(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	// Find pilot tone.
	proceed_to_symbol(tape, SymbolType::Pilot);
	if(is_at_end(tape)) return std::nullopt;

	// Find sync.
	proceed_to_symbol(tape, SymbolType::Sync);
	if(is_at_end(tape)) return std::nullopt;

	// Read market byte.
	const auto type = get_byte(tape);
	if(!type) return std::nullopt;
	if(*type != 0x00) return std::nullopt;
	reset_checksum();

	// Read header contents.
	uint8_t header_bytes[17];
	for(size_t c = 0; c < sizeof(header_bytes); c++) {
		const auto next_byte = get_byte(tape);
		if(!next_byte) return std::nullopt;
		header_bytes[c] = *next_byte;
	}

	// Check checksum.
	const auto post_checksum = get_byte(tape);
	if(!post_checksum || *post_checksum) return std::nullopt;

	// Unpack and return.
	Header header;
	header.type = header_bytes[0];
	memcpy(&header.name, &header_bytes[1], 10);
	header.data_length = uint16_t(header_bytes[11] | (header_bytes[12] << 8));
	header.parameters[0] = uint16_t(header_bytes[13] | (header_bytes[14] << 8));
	header.parameters[1] = uint16_t(header_bytes[15] | (header_bytes[16] << 8));
	return header;
}

void Parser::reset_checksum() {
	checksum_ = 0;
}

std::optional<uint8_t> Parser::get_byte(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	uint8_t result = 0;
	for(int c = 0; c < 8; c++) {
		const SymbolType symbol = get_next_symbol(tape);
		if(symbol != SymbolType::One && symbol != SymbolType::Zero) return std::nullopt;
		result = uint8_t((result << 1) | (symbol == SymbolType::One));
	}
	checksum_ ^= result;
	return result;
}
