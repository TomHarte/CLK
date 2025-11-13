//
//  SID.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Numeric/SizedInt.hpp"

#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "SignalProcessing/BiquadFilter.hpp"

namespace MOS::SID {

struct Voice {
	static constexpr uint16_t MaxWaveformValue = (1 << 12) - 1;

	struct Oscillator {
		// Programmer inputs.
		uint32_t pitch = 0;
		uint32_t pulse_width = 0;

		// State.
		//
		// A real SID has a 24-bit phase counter and does various things when the top bit transitions from 0 to 1.
		// This implementation maintains a 32-bit phase counter in which the low byte is unused and the top bit
		// is inverted. That saves the cost of any masking and makes the 0 -> 1 transition test actually a 1 -> 0
		// transition test, which can be phrased simply as after < before. Sadly overflow of signed integers is
		// still undefined behaviour in C++ at the time of writing.
		static constexpr uint32_t PhaseReload = 0x8000'0000;
		uint32_t phase = PhaseReload;
		uint32_t previous_phase = PhaseReload;

		void reset_phase();
		bool did_raise_b23() const;
		bool did_raise_b19() const;
		uint16_t sawtooth_output() const;
	} oscillator;
	struct ADSR {
		// Programmer inputs.
		Numeric::SizedInt<4> attack;
		Numeric::SizedInt<4> decay;
		Numeric::SizedInt<4> release;

		Numeric::SizedInt<8> sustain;

		// State.
		enum class Phase {
			Attack,
			DecayAndHold,
			Release,
		} phase = Phase::Release;
		Numeric::SizedInt<15> rate_counter;
		Numeric::SizedInt<15> rate_counter_target;

		uint8_t exponential_counter;
		uint8_t envelope;

		void set_phase(const Phase);
	} adsr;
	struct NoiseGenerator {
		static constexpr uint32_t NoiseReload = 0x7'ffff;
		uint32_t noise = NoiseReload;

		uint16_t output() const;
		void update(const bool test);
	} noise_generator;

	void set_control(const uint8_t);
	void update();
	void synchronise(const Voice &prior);
	uint16_t output(const Voice &prior) const;

private:
	Numeric::SizedInt<8> control;
	bool noise() const;
	bool pulse() const;
	bool sawtooth() const;
	bool triangle() const;
	bool test() const;
	bool ring_mod() const;
	bool sync() const;
	bool gate() const;

	uint16_t pulse_output() const;
	uint16_t triangle_output(const Voice &prior) const;
};

class SID: public Outputs::Speaker::BufferSource<SID, false> {
public:
	SID(Concurrency::AsyncTaskQueue<false> &audio_queue);

	void write(Numeric::SizedInt<5> address, uint8_t value);
	uint8_t read(Numeric::SizedInt<5> address);

	// Outputs::Speaker::BufferSource.
	template <Outputs::Speaker::Action action>
		void apply_samples(std::size_t, Outputs::Speaker::MonoSample *);
	bool is_zero_level() const;
	void set_sample_volume_range(std::int16_t);

private:
	Concurrency::AsyncTaskQueue<false> &audio_queue_;
	Voice voices_[3];

	uint8_t last_write_;

	int16_t range_ = 0;
	uint8_t volume_ = 0;

	SignalProcessing::BiquadFilter filter_;
	Numeric::SizedInt<11> filter_cutoff_;
	Numeric::SizedInt<4> filter_resonance_;
	Numeric::SizedInt<4> filter_channels_;
	Numeric::SizedInt<3> filter_mode_;
	void update_filter();

	SignalProcessing::BiquadFilter output_filter_;
};

}
