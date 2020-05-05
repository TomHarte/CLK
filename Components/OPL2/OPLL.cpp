//
//  OPLL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "OPLL.hpp"

#include <cassert>

using namespace Yamaha::OPL;

OPLL::OPLL(Concurrency::DeferringAsyncTaskQueue &task_queue, int audio_divider, bool is_vrc7):
	OPLBase(task_queue), audio_divider_(audio_divider), is_vrc7_(is_vrc7) {
	// Due to the way that sound mixing works on the OPLL, the audio divider may not
	// be larger than 4.
	assert(audio_divider <= 4);

	// Set up proper damping management.
	for(int c = 0; c < 9; ++c) {
		envelope_generators_[c].set_should_damp([this, c] {
			// Propagate attack mode to the modulator, and reset both phases.
			envelope_generators_[c + 9].set_key_on(true);
			phase_generators_[c + 0].reset();
			phase_generators_[c + 9].reset();
		});
	}
}

// MARK: - Machine-facing programmatic input.

void OPLL::write_register(uint8_t address, uint8_t value) {
	// The OPLL doesn't have timers or other non-audio functions, so all writes
	// go to the audio queue.
	task_queue_.defer([this, address, value] {
		// The first 8 locations are used to define the custom instrument, and have
		// exactly the same format as the patch set arrays at the head of this file.
		if(address < 8) {
			custom_instrument_[address] = value;

			// Update all channels that refer to instrument 0.
			for(int c = 0; c < 9; ++c) {
				if(!channels_[c].instrument) {
					install_instrument(c);
				}
			}

			return;
		}

		// Register 0xe enables or disables rhythm mode and contains the
		// percussion key-on bits.
		if(address == 0xe) {
			rhythm_mode_enabled_ = value & 0x20;
			rhythm_generators_[0].set_key_on(value & 0x01);
			rhythm_generators_[1].set_key_on(value & 0x02);
			rhythm_generators_[2].set_key_on(value & 0x04);
			rhythm_generators_[3].set_key_on(value & 0x08);
			rhythm_generators_[4].set_key_on(value & 0x10);
			return;
		}

		// That leaves only per-channel selections, for which the addressing
		// is completely orthogonal; check that a valid channel is being requested.
		const auto index = address & 0xf;
		if(index > 8) return;

		switch(address & 0xf0) {
			default: break;

			// Address 1x sets the low 8 bits of the period for channel x.
			case 0x10:
				channels_[index].period = (channels_[index].period & ~0xff) | value;
				set_channel_period(index);
			return;

			// Address 2x Sets the octave and a single bit of the frequency, as well
			// as setting key on and sustain mode.
			case 0x20:
				channels_[index].period = (channels_[index].period & 0xff) | ((value & 1) << 8);
				channels_[index].octave = (value >> 1) & 7;
				set_channel_period(index);

				// In this implementation the first 9 envelope generators are for
				// channel carriers, and their will_attack callback is used to trigger
				// key-on for modulators. But key-off needs to be set to both envelope
				// generators now.
				if(value & 0x10) {
					envelope_generators_[index].set_key_on(true);
				} else {
					envelope_generators_[index + 0].set_key_on(false);
					envelope_generators_[index + 9].set_key_on(false);
				}

				// Set sustain bit to both the relevant operators.
				channels_[index].use_sustain = value & 0x20;
				set_use_sustain(index);
			return;

			// Address 3x selects the instrument and attenuation for a channel;
			// in rhythm mode some of the nibbles that ordinarily identify instruments
			// instead nominate additional attenuations. This code reads those back
			// from the stored instrument values.
			case 0x30:
				channels_[index].instrument = value >> 4;
				channels_[index].attenuation = value >> 4;
				install_instrument(index);
			return;
		}
	});
}

void OPLL::set_channel_period(int channel) {
	phase_generators_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	phase_generators_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);

	envelope_generators_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	envelope_generators_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);

	key_level_scalers_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	key_level_scalers_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);
}

const uint8_t *OPLL::instrument_definition(int instrument) {
	// Instrument 0 is the custom instrument.
	if(!instrument) return custom_instrument_;

	// Instruments other than 0 are taken from the fixed set.
	const int index = (instrument - 1) * 8;
	return is_vrc7_ ? &vrc7_patch_set[index] : &opll_patch_set[index];
}

void OPLL::install_instrument(int channel) {
	auto &carrier_envelope = envelope_generators_[channel + 0];
	auto &carrier_phase = phase_generators_[channel + 0];
	auto &carrier_scaler = key_level_scalers_[channel + 0];

	auto &modulator_envelope = envelope_generators_[channel + 9];
	auto &modulator_phase = phase_generators_[channel + 9];
	auto &modulator_scaler = key_level_scalers_[channel + 9];

	const uint8_t *const instrument = instrument_definition(channels_[channel].instrument);

	// Bytes 0 (modulator) and 1 (carrier):
	//
	//	b0-b3:	multiplier;
	//	b4:		key-scale rate enable;
	//	b5:		sustain-level enable;
	//	b6:		vibrato enable;
	//	b7:		tremolo enable.
	modulator_phase.set_multiple(instrument[0] & 0xf);
	channels_[channel].modulator_key_rate_scale_multiplier = (instrument[0] >> 4) & 1;
	modulator_phase.set_vibrato_enabled(instrument[0] & 0x40);
	modulator_envelope.set_tremolo_enabled(instrument[0] & 0x80);

	carrier_phase.set_multiple(instrument[1] & 0xf);
	channels_[channel].carrier_key_rate_scale_multiplier = (instrument[1] >> 4) & 1;
	carrier_phase.set_vibrato_enabled(instrument[1] & 0x40);
	carrier_envelope.set_tremolo_enabled(instrument[1] & 0x80);

	// Pass off bit 5.
	set_use_sustain(channel);

	// Byte 2:
	//
	//	b0–b5:	modulator attenuation;
	//	b6–b7:	modulator key-scale level.
	modulator_scaler.set_key_scaling_level(instrument[3] >> 6);
	channels_[channel].modulator_attenuation = instrument[2] & 0x3f;

	// Byte 3:
	//
	//	b0–b2:	modulator feedback level;
	//	b3:		modulator waveform selection;
	//	b4:		carrier waveform selection;
	//	b5:		[unused]
	//	b6–b7:	carrier key-scale level.
	channels_[channel].modulator_feedback = instrument[3] & 7;
	channels_[channel].modulator_waveform = Waveform((instrument[3] >> 3) & 1);
	channels_[channel].carrier_waveform = Waveform((instrument[3] >> 4) & 1);
	carrier_scaler.set_key_scaling_level(instrument[3] >> 6);

	// Bytes 4 (modulator) and 5 (carrier):
	//
	//	b0–b3:	decay rate;
	//	b4–b7:	attack rate.
	modulator_envelope.set_decay_rate(instrument[4] & 0xf);
	modulator_envelope.set_attack_rate(instrument[4] >> 4);
	carrier_envelope.set_decay_rate(instrument[5] & 0xf);
	carrier_envelope.set_attack_rate(instrument[5] >> 4);

	// Bytes 6 (modulator) and 7 (carrier):
	//
	//	b0–b3:	release rate;
	//	b4–b7:	sustain level.
	modulator_envelope.set_release_rate(instrument[6] & 0xf);
	modulator_envelope.set_sustain_level(instrument[6] >> 4);
	carrier_envelope.set_release_rate(instrument[7] & 0xf);
	carrier_envelope.set_release_rate(instrument[7] >> 4);
}

void OPLL::set_use_sustain(int channel) {
	const uint8_t *const instrument = instrument_definition(channels_[channel].instrument);
	envelope_generators_[channel + 0].set_sustain_level((instrument[1] & 0x20) || channels_[channel].use_sustain);
	envelope_generators_[channel + 9].set_sustain_level((instrument[0] & 0x20) || channels_[channel].use_sustain);
}

// MARK: - Output generation.

void OPLL::set_sample_volume_range(std::int16_t range) {
	total_volume_ = range;
}

void OPLL::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// Both the OPLL and the OPL2 divide the input clock by 72 to get the base tick frequency;
	// unlike the OPL2 the OPLL time-divides the output for 'mixing'.

	const int update_period = 72 / audio_divider_;
	const int channel_output_period = 4 / audio_divider_;

	// TODO: the conditional below is terrible. Fix.
	while(number_of_samples--) {
		if(!audio_offset_) update_all_channels();

		*target = output_levels_[audio_offset_ / channel_output_period];
		++target;
		audio_offset_ = (audio_offset_ + 1) % update_period;
	}
}

void OPLL::update_all_channels() {
	oscillator_.update();

	// Update all phase generators. That's guaranteed.
	for(int c = 0; c < 18; ++c) {
		phase_generators_[c].update(oscillator_);
	}

	// Update the ADSR envelopes that are guaranteed to be melodic.
	for(int c = 0; c < 6; ++c) {
		envelope_generators_[c + 0].update(oscillator_);
		envelope_generators_[c + 9].update(oscillator_);
	}

#define VOLUME(x)	int16_t(((x) * total_volume_) >> 12)

	if(rhythm_mode_enabled_) {
		// Advance the rhythm envelope generators.
		// TODO: these need to be properly seeded.
		for(int c = 0; c < 5; ++c) {
			oscillator_.update_lfsr();
			rhythm_generators_[c].update(oscillator_);
		}

		// Fill in the melodic channels.
		output_levels_[3] = VOLUME(melodic_output(0));
		output_levels_[4] = VOLUME(melodic_output(1));
		output_levels_[5] = VOLUME(melodic_output(2));

		output_levels_[9] = VOLUME(melodic_output(3));
		output_levels_[10] = VOLUME(melodic_output(4));
		output_levels_[11] = VOLUME(melodic_output(5));

		// TODO: drum noises. Also subject to proper channel population.
	} else {
		for(int c = 6; c < 9; ++c) {
			envelope_generators_[c + 0].update(oscillator_);
			envelope_generators_[c + 9].update(oscillator_);
		}

		// All melodic. Fairly easy.
		output_levels_[0] = output_levels_[1] = output_levels_[2] =
		output_levels_[6] = output_levels_[7] = output_levels_[8] =
		output_levels_[12] = output_levels_[13] = output_levels_[14] = 0;

		output_levels_[3] = VOLUME(melodic_output(0));
		output_levels_[4] = VOLUME(melodic_output(1));
		output_levels_[5] = VOLUME(melodic_output(2));

		output_levels_[9] = VOLUME(melodic_output(3));
		output_levels_[10] = VOLUME(melodic_output(4));
		output_levels_[11] = VOLUME(melodic_output(5));

		output_levels_[15] = VOLUME(melodic_output(6));
		output_levels_[16] = VOLUME(melodic_output(7));
		output_levels_[17] = VOLUME(melodic_output(8));

		// TODO: advance LFSR.
	}

#undef VOLUME

	// TODO: batch updates of the LFSR.
	// TODO: modulator feedback.
}


int OPLL::melodic_output(int channel) {
	// TODO: key-rate scaling.
	// TODO: proper scales of all attenuations below.
	auto modulation = WaveformGenerator<period_precision>::wave(channels_[channel].modulator_waveform, phase_generators_[channel + 9].phase());
	modulation += envelope_generators_[channel + 9].attenuation() + channels_[channel].modulator_attenuation;

	return WaveformGenerator<period_precision>::wave(channels_[channel].carrier_waveform, phase_generators_[channel].scaled_phase(), modulation).level() + channels_[channel].attenuation;
}
