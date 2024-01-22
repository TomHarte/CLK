//
//  OPLL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "Implementation/OPLBase.hpp"
#include "Implementation/EnvelopeGenerator.hpp"
#include "Implementation/KeyLevelScaler.hpp"
#include "Implementation/PhaseGenerator.hpp"
#include "Implementation/LowFrequencyOscillator.hpp"
#include "Implementation/WaveformGenerator.hpp"

#include <atomic>

namespace Yamaha::OPL {

class OPLL: public OPLBase<OPLL> {
	public:
		/// Creates a new OPLL or VRC7.
		OPLL(Concurrency::AsyncTaskQueue<false> &task_queue, int audio_divider = 1, bool is_vrc7 = false);

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);

		// The OPLL is generally 'half' as loud as it's told to be. This won't strictly be true in
		// rhythm mode, but it's correct for melodic output.
		double get_average_output_peak() const { return 0.5; }

		/// Reads from the OPL.
		uint8_t read(uint16_t address);

	private:
		friend OPLBase<OPLL>;
		void write_register(uint8_t address, uint8_t value);

		int audio_divider_ = 0;
		int audio_offset_ = 0;
		std::atomic<int> total_volume_;

		int16_t output_levels_[18];
		void update_all_channels();

		int melodic_output(int channel);
		int bass_drum();
		int tom_tom();
		int snare_drum();
		int cymbal();
		int high_hat();

		static constexpr int period_precision = 9;
		static constexpr int envelope_precision = 7;

		// Standard melodic phase and envelope generators;
		//
		// These are assigned as:
		//
		//		[x], 0 <= x < 9		= carrier for channel x;
		//		[x+9]				= modulator for channel x.
		//
		PhaseGenerator<period_precision> phase_generators_[18];
		EnvelopeGenerator<envelope_precision, period_precision> envelope_generators_[18];
		KeyLevelScaler<period_precision> key_level_scalers_[18];

		// Dedicated rhythm envelope generators and attenuations.
		EnvelopeGenerator<envelope_precision, period_precision> rhythm_envelope_generators_[6];
		enum RhythmIndices {
			HighHat = 0,
			Cymbal = 1,
			TomTom = 2,
			Snare = 3,
			BassCarrier = 4,
			BassModulator = 5
		};

		// Channel specifications.
		struct Channel {
			int octave = 0;
			int period = 0;
			int instrument = 0;

			int attenuation = 0;
			int modulator_attenuation = 0;

			Waveform carrier_waveform = Waveform::Sine;
			Waveform modulator_waveform = Waveform::Sine;

			int carrier_key_rate_scale_multiplier = 0;
			int modulator_key_rate_scale_multiplier = 0;

			LogSign modulator_output;
			int modulator_feedback = 0;

			bool use_sustain = false;
		} channels_[9];

		// The low-frequency oscillator.
		LowFrequencyOscillator oscillator_;
		bool rhythm_mode_enabled_ = false;
		bool is_vrc7_ = false;

		// Contains the current configuration of the custom instrument.
		uint8_t custom_instrument_[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		// Helpers to push per-channel information.

		/// Pushes the current octave and period to channel @c channel.
		void set_channel_period(int channel);

		/// Installs the appropriate instrument on channel @c channel.
		void install_instrument(int channel);

		/// Sets whether the sustain level is used for channel @c channel based on its current instrument
		/// and the user's selection.
		void set_use_sustain(int channel);

		/// @returns The 8-byte definition of instrument @c instrument.
		const uint8_t *instrument_definition(int instrument, int channel);
};

}
