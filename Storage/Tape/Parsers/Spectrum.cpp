//
//  Spectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Spectrum.hpp"

#include "../../../Numeric/CRC.hpp"

#include <cstring>

//
// Sources used for the logic below:
//
//		https://sinclair.wiki.zxnet.co.uk/wiki/Spectrum_tape_interface
//		http://www.cpctech.cpc-live.com/docs/manual/s968se08.pdf
//		https://www.alessandrogrussu.it/tapir/tzxform120.html
//

using namespace Storage::Tape::ZXSpectrum;

Parser::Parser(MachineType machine_type) :
	machine_type_(machine_type) {}

void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	if(pulse.type == Storage::Tape::Tape::Pulse::Type::Zero) {
		push_wave(WaveType::Gap);
		return;
	}

	// Only pulse duration matters; the ZX Spectrum et al do not rely on polarity.
	const float t_states = pulse.length.get<float>() * 3'500'000.0f;

	switch(speed_phase_) {
		case SpeedDetectionPhase::WaitingForGap:
			// A gap is: any 'pulse' of at least 3000 t-states.
			if(t_states >= 3000.0f) {
				speed_phase_ = SpeedDetectionPhase::WaitingForPilot;
			}
		return;

		case SpeedDetectionPhase::WaitingForPilot:
			// Pilot tone might be: any pulse of less than 3000 t-states.
			if(t_states >= 3000.0f) return;
			speed_phase_ = SpeedDetectionPhase::CalibratingPilot;
			calibration_pulse_pointer_ = 0;
		[[fallthrough]];

		case SpeedDetectionPhase::CalibratingPilot: {
			// Pilot calibration: await at least 8 consecutive pulses of similar length.
			calibration_pulses_[calibration_pulse_pointer_] = t_states;
			++calibration_pulse_pointer_;

			// Decide whether it looks like this isn't actually pilot tone.
			float mean = 0.0f;
			for(size_t c = 0; c < calibration_pulse_pointer_; c++) {
				mean += calibration_pulses_[c];
			}
			mean /= float(calibration_pulse_pointer_);
			for(size_t c = 0; c < calibration_pulse_pointer_; c++) {
				if(calibration_pulses_[c] < mean * 0.9f || calibration_pulses_[c] > mean * 1.1f) {
					speed_phase_ = SpeedDetectionPhase::WaitingForGap;
					return;
				}
			}

			// Advance only if 8 are present.
			if(calibration_pulse_pointer_ == calibration_pulses_.size()) {
				speed_phase_ = SpeedDetectionPhase::Done;

				// Note at least one full cycle of pilot tone.
				push_wave(WaveType::Pilot);
				push_wave(WaveType::Pilot);

				// Configure proper parameters for the autodetection machines.
				switch(machine_type_) {
					default: break;

					case MachineType::AmstradCPC:
						// CPC: pilot tone is length of bit 1; bit 0 is half that.
						// So no more detecting formal pilot waves.
						set_cpc_one_zero_boundary(mean * 0.75f);
					break;

					case MachineType::Enterprise:
						// There's a third validation check here: is this one of the two
						// permitted recording speeds?
						if(!(
								(mean >= 742.0f*0.9f && mean <= 742.0f*1.0f/0.9f) ||
								(mean >= 1750.0f*0.9f && mean <= 1750.0f*1.0f/0.9f)
							)) {
							speed_phase_ = SpeedDetectionPhase::WaitingForGap;
							return;
						}

						// TODO: not yet supported. As below, needs to deal with sync != zero.
						assert(false);
					break;

					case MachineType::SAMCoupe: {
						// TODO: not yet supported. Specifically because I don't think my sync = zero
						// assumption even vaguely works here?
						assert(false);
					} break;
				}
			}
		} return;

		default:
		break;
	}

	// Too long or too short => gap.
	if(t_states >= too_long_ || t_states <= too_short_) {
		push_wave(WaveType::Gap);
		return;
	}

	// Potentially announce pilot.
	if(t_states >= is_pilot_) {
		push_wave(WaveType::Pilot);
		return;
	}

	// Otherwise it's either a one or a zero.
	push_wave(t_states > is_one_ ? WaveType::One : WaveType::Zero);
}

void Parser::set_cpc_read_speed(uint8_t speed) {
	// This may not be exactly right; I wish there were more science here but
	// instead it's empirical based on tape speed versus value stored plus
	// a guess as to where the CPC puts the dividing line.
	set_cpc_one_zero_boundary(float(speed) * 14.35f);
}

void Parser::set_cpc_one_zero_boundary(float boundary) {
	is_one_ = boundary;
	too_long_ = is_one_ * 16.0f / 9.0f;
	too_short_ = is_one_ * 0.5f;
	is_pilot_ = too_long_;
}

void Parser::inspect_waves(const std::vector<Storage::Tape::ZXSpectrum::WaveType> &waves) {
	switch(waves[0]) {
		// Gap and Pilot map directly.
		case WaveType::Gap:		push_symbol(SymbolType::Gap, 1);	break;
		case WaveType::Pilot:	push_symbol(SymbolType::Pilot, 1);	break;

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

std::optional<Block> Parser::find_block(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	// Decide whether to kick off a speed detection phase.
	if(should_detect_speed()) {
		speed_phase_ = SpeedDetectionPhase::WaitingForGap;
	}

	// Find pilot tone.
	proceed_to_symbol(tape, SymbolType::Pilot);
	if(is_at_end(tape)) return std::nullopt;

	// Find sync bit.
	proceed_to_symbol(tape, SymbolType::Zero);
	if(is_at_end(tape)) return std::nullopt;

	// Read marker byte.
	const auto type = get_byte(tape);
	if(!type) return std::nullopt;

	// That succeeded.
	Block block = {
		.type = *type
	};
	return block;
}

std::vector<uint8_t> Parser::get_block_body(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::vector<uint8_t> result;

	while(true) {
		const auto next_byte = get_byte(tape);
		if(!next_byte) break;
		result.push_back(*next_byte);
	}

	return result;
}

void Parser::seed_checksum(uint8_t value) {
	checksum_ = value;
}

std::optional<uint8_t> Parser::get_byte(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	uint8_t result = 0;
	for(int c = 0; c < 8; c++) {
		const SymbolType symbol = get_next_symbol(tape);
		if(symbol != SymbolType::One && symbol != SymbolType::Zero) return std::nullopt;
		result = uint8_t((result << 1) | (symbol == SymbolType::One));
	}

	if(should_flip_bytes()) {
		result = CRC::reverse_byte(result);
	}

	checksum_ ^= result;
	return result;
}
