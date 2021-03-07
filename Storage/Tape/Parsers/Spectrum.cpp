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
