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

OPLL::OPLL(Concurrency::AsyncTaskQueue<false> &task_queue, const int audio_divider, const bool is_vrc7):
	OPLBase(task_queue), audio_divider_(audio_divider), is_vrc7_(is_vrc7) {
	// Due to the way that sound mixing works on the OPLL, the audio divider may not
	// be larger than 4.
	assert(audio_divider <= 4);

	// Setup the rhythm envelope generators.

	// Treat the bass exactly as if it were a melodic channel.
	rhythm_envelope_generators_[BassCarrier].set_should_damp([this] {
		// Propagate attack mode to the modulator, and reset both phases.
		rhythm_envelope_generators_[BassModulator].set_key_on(true);
		phase_generators_[6 + 0].reset();
		phase_generators_[6 + 9].reset();
	});

	// Set the other drums to damp, but only the TomTom to affect phase.
	rhythm_envelope_generators_[TomTom].set_should_damp([this] {
		phase_generators_[8 + 9].reset();
	});
	rhythm_envelope_generators_[Snare].set_should_damp({});
	rhythm_envelope_generators_[Cymbal].set_should_damp({});
	rhythm_envelope_generators_[HighHat].set_should_damp({});

	// Crib the proper rhythm envelope generator settings by installing
	// the rhythm instruments and copying them over.
	rhythm_mode_enabled_ = true;
	install_instrument(6);
	install_instrument(7);
	install_instrument(8);

	rhythm_envelope_generators_[BassCarrier] = envelope_generators_[6];
	rhythm_envelope_generators_[BassModulator] = envelope_generators_[6 + 9];
	rhythm_envelope_generators_[HighHat] = envelope_generators_[7 + 9];
	rhythm_envelope_generators_[Cymbal] = envelope_generators_[8];
	rhythm_envelope_generators_[TomTom] = envelope_generators_[8 + 9];
	rhythm_envelope_generators_[Snare] = envelope_generators_[7];

	// Return to ordinary default mode.
	rhythm_mode_enabled_ = false;

	// Set up damping for the melodic channels.
	for(int c = 0; c < 9; ++c) {
		envelope_generators_[c].set_should_damp([this, c] {
			// Propagate attack mode to the modulator, and reset both phases.
			envelope_generators_[c + 9].set_key_on(true);
			phase_generators_[c + 0].reset();
			phase_generators_[c + 9].reset();
		});
	}

	// Set default instrument.
	for(int c = 0; c < 9; ++c) {
		install_instrument(c);
	}
}

// MARK: - Machine-facing programmatic input.

void OPLL::write_register(const uint8_t address, const uint8_t value) {
	// The OPLL doesn't have timers or other non-audio functions, so all writes
	// go to the audio queue.
	task_queue_.enqueue([this, address, value] {
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
			const bool old_rhythm_mode = rhythm_mode_enabled_;
			rhythm_mode_enabled_ = value & 0x20;
			if(old_rhythm_mode != rhythm_mode_enabled_) {
				// Change the instlled instruments for channels 6, 7 and 8
				// if this was a transition into or out of rhythm mode.
				install_instrument(6);
				install_instrument(7);
				install_instrument(8);
			}
			rhythm_envelope_generators_[HighHat].set_key_on(value & 0x01);
			rhythm_envelope_generators_[Cymbal].set_key_on(value & 0x02);
			rhythm_envelope_generators_[TomTom].set_key_on(value & 0x04);
			rhythm_envelope_generators_[Snare].set_key_on(value & 0x08);
			if(value & 0x10) {
				rhythm_envelope_generators_[BassCarrier].set_key_on(true);
			} else {
				rhythm_envelope_generators_[BassCarrier].set_key_on(false);
				rhythm_envelope_generators_[BassModulator].set_key_on(false);

			}
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
				channels_[index].attenuation = value & 0xf;

				// Install an instrument only if it's new.
				if(channels_[index].instrument != value >> 4) {
					channels_[index].instrument = value >> 4;
					if(index < 6 || !rhythm_mode_enabled_) {
						install_instrument(index);
					}
				}
			return;
		}
	});
}

void OPLL::set_channel_period(const int channel) {
	phase_generators_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	phase_generators_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);

	envelope_generators_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	envelope_generators_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);

	key_level_scalers_[channel + 0].set_period(channels_[channel].period, channels_[channel].octave);
	key_level_scalers_[channel + 9].set_period(channels_[channel].period, channels_[channel].octave);
}

const uint8_t *OPLL::instrument_definition(const int instrument, const int channel) const {
	// Divert to the appropriate rhythm instrument if in rhythm mode.
	if(channel >= 6 && rhythm_mode_enabled_) {
		return &percussion_patch_set[(channel - 6) * 8];
	}

	// Instrument 0 is the custom instrument.
	if(!instrument) return custom_instrument_;

	// Instruments other than 0 are taken from the fixed set.
	const int index = (instrument - 1) * 8;
	return is_vrc7_ ? &vrc7_patch_set[index] : &opll_patch_set[index];
}

void OPLL::install_instrument(const int channel) {
	auto &carrier_envelope = envelope_generators_[channel + 0];
	auto &carrier_phase = phase_generators_[channel + 0];
	auto &carrier_scaler = key_level_scalers_[channel + 0];

	auto &modulator_envelope = envelope_generators_[channel + 9];
	auto &modulator_phase = phase_generators_[channel + 9];
	auto &modulator_scaler = key_level_scalers_[channel + 9];

	const uint8_t *const instrument = instrument_definition(channels_[channel].instrument, channel);

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
	carrier_envelope.set_sustain_level(instrument[7] >> 4);
}

void OPLL::set_use_sustain(const int channel) {
	const uint8_t *const instrument = instrument_definition(channels_[channel].instrument, channel);
	envelope_generators_[channel + 0].set_use_sustain_level((instrument[1] & 0x20) || channels_[channel].use_sustain);
	envelope_generators_[channel + 9].set_use_sustain_level((instrument[0] & 0x20) || channels_[channel].use_sustain);
}

// MARK: - Output generation.

void OPLL::set_sample_volume_range(const std::int16_t range) {
	total_volume_ = range;
}

template <Outputs::Speaker::Action action>
void OPLL::apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target) {
	// Both the OPLL and the OPL2 divide the input clock by 72 to get the base tick frequency;
	// unlike the OPL2 the OPLL time-divides the output for 'mixing'.

	const int update_period = 72 / audio_divider_;
	const int channel_output_period = 4 / audio_divider_;

	// TODO: the conditional below is terrible. Fix.
	while(number_of_samples--) {
		if(!audio_offset_) update_all_channels();

		Outputs::Speaker::apply<action>(*target, output_levels_[audio_offset_ / channel_output_period]);
		++target;
		audio_offset_ = (audio_offset_ + 1) % update_period;
	}
}

template void OPLL::apply_samples<Outputs::Speaker::Action::Mix>(std::size_t, Outputs::Speaker::MonoSample *);
template void OPLL::apply_samples<Outputs::Speaker::Action::Store>(std::size_t, Outputs::Speaker::MonoSample *);
template void OPLL::apply_samples<Outputs::Speaker::Action::Ignore>(std::size_t, Outputs::Speaker::MonoSample *);

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

	const auto volume = [&](const int x) {
		return int16_t(
			(x * total_volume_) >> 12
		);
	};

	if(rhythm_mode_enabled_) {
		// Advance the rhythm envelope generators.
		for(int c = 0; c < 6; ++c) {
			rhythm_envelope_generators_[c].update(oscillator_);
		}

		// Fill in the melodic channels.
		output_levels_[3] = volume(melodic_output(0));
		output_levels_[4] = volume(melodic_output(1));
		output_levels_[5] = volume(melodic_output(2));

		output_levels_[9] = volume(melodic_output(3));
		output_levels_[10] = volume(melodic_output(4));
		output_levels_[11] = volume(melodic_output(5));

		// Bass drum, which is a regular FM effect.
		output_levels_[2] = output_levels_[15] = volume(bass_drum());
		oscillator_.update_lfsr();

		// Tom tom, which is a single operator.
		output_levels_[1] = output_levels_[14] = volume(tom_tom());
		oscillator_.update_lfsr();

		// Snare.
		output_levels_[6] = output_levels_[16] = volume(snare_drum());
		oscillator_.update_lfsr();

		// Cymbal.
		output_levels_[7] = output_levels_[17] = volume(cymbal());
		oscillator_.update_lfsr();

		// High-hat.
		output_levels_[0] = output_levels_[13] = volume(high_hat());
		oscillator_.update_lfsr();

		// Unutilised slots.
		output_levels_[8] = output_levels_[12] = 0;
		oscillator_.update_lfsr();
	} else {
		for(int c = 6; c < 9; ++c) {
			envelope_generators_[c + 0].update(oscillator_);
			envelope_generators_[c + 9].update(oscillator_);
		}

		// All melodic. Fairly easy.
		output_levels_[0] = output_levels_[1] = output_levels_[2] =
		output_levels_[6] = output_levels_[7] = output_levels_[8] =
		output_levels_[12] = output_levels_[13] = output_levels_[14] = 0;

		output_levels_[3] = volume(melodic_output(0));
		output_levels_[4] = volume(melodic_output(1));
		output_levels_[5] = volume(melodic_output(2));

		output_levels_[9] = volume(melodic_output(3));
		output_levels_[10] = volume(melodic_output(4));
		output_levels_[11] = volume(melodic_output(5));

		output_levels_[15] = volume(melodic_output(6));
		output_levels_[16] = volume(melodic_output(7));
		output_levels_[17] = volume(melodic_output(8));
	}

	// TODO: batch updates of the LFSR.
}

namespace {

// TODO: verify attenuation scales pervasively below.
constexpr int attenuation(const int x) {
	return x << 7;
}

}

int OPLL::melodic_output(const int channel) {
	// The modulator always updates after the carrier, oddly enough. So calculate actual output first, based on the modulator's last value.
	auto carrier = WaveformGenerator<period_precision>::wave(channels_[channel].carrier_waveform, phase_generators_[channel].scaled_phase(), channels_[channel].modulator_output);
	carrier += envelope_generators_[channel].attenuation() + attenuation(channels_[channel].attenuation) + key_level_scalers_[channel].attenuation();

	// Get the modulator's new value.
	auto modulation = WaveformGenerator<period_precision>::wave(channels_[channel].modulator_waveform, phase_generators_[channel + 9].phase());
	modulation += envelope_generators_[channel + 9].attenuation() + (channels_[channel].modulator_attenuation << 5) + key_level_scalers_[channel + 9].attenuation();

	// Apply feedback, if any.
	phase_generators_[channel + 9].apply_feedback(channels_[channel].modulator_output, modulation, channels_[channel].modulator_feedback);
	channels_[channel].modulator_output = modulation;

	return carrier.level();
}

int OPLL::bass_drum() const {
	// Use modulator 6 and carrier 6, attenuated as per the bass-specific envelope generators and the attenuation level for channel 6.
	auto modulation = WaveformGenerator<period_precision>::wave(Waveform::Sine, phase_generators_[6 + 9].phase());
	modulation += rhythm_envelope_generators_[RhythmIndices::BassModulator].attenuation();

	auto carrier = WaveformGenerator<period_precision>::wave(Waveform::Sine, phase_generators_[6].scaled_phase(), modulation);
	carrier += rhythm_envelope_generators_[RhythmIndices::BassCarrier].attenuation() + attenuation(channels_[6].attenuation);
	return carrier.level();
}

int OPLL::tom_tom() const {
	// Use modulator 8 and the 'instrument' selection for channel 8 as an attenuation.
	auto tom_tom = WaveformGenerator<period_precision>::wave(Waveform::Sine, phase_generators_[8 + 9].phase());
	tom_tom += rhythm_envelope_generators_[RhythmIndices::TomTom].attenuation();
	tom_tom += attenuation(channels_[8].instrument);
	return tom_tom.level();
}

int OPLL::snare_drum() const {
	// Use modulator 7 and the carrier attenuation level for channel 7.
	LogSign snare = WaveformGenerator<period_precision>::snare(oscillator_, phase_generators_[7 + 9].phase());
	snare += rhythm_envelope_generators_[RhythmIndices::Snare].attenuation();
	snare += attenuation(channels_[7].attenuation);
	return snare.level();
}

int OPLL::cymbal() const {
	// Use modulator 7, carrier 8 and the attenuation level for channel 8.
	LogSign cymbal = WaveformGenerator<period_precision>::cymbal(phase_generators_[8].phase(), phase_generators_[7 + 9].phase());
	cymbal += rhythm_envelope_generators_[RhythmIndices::Cymbal].attenuation();
	cymbal += attenuation(channels_[8].attenuation);
	return cymbal.level();
}

int OPLL::high_hat() const {
	// Use modulator 7, carrier 8 a and the 'instrument' selection for channel 7 as an attenuation.
	LogSign high_hat = WaveformGenerator<period_precision>::high_hat(oscillator_, phase_generators_[8].phase(), phase_generators_[7 + 9].phase());
	high_hat += rhythm_envelope_generators_[RhythmIndices::HighHat].attenuation();
	high_hat += attenuation(channels_[7].instrument);
	return high_hat.level();
}
