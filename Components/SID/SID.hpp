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

namespace MOS::SID {

class SID: public Outputs::Speaker::BufferSource<SID, false> {
public:
	SID(Concurrency::AsyncTaskQueue<false> &audio_queue);

	void write(Numeric::SizedInt<5> address, uint8_t value);
	uint8_t read(Numeric::SizedInt<5> address);

	// Outputs::Speaker::BufferSource.
	template <Outputs::Speaker::Action action>
		void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
	bool is_zero_level() const;
	void set_sample_volume_range(std::int16_t);

private:
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	struct Voice {
		struct Oscillator {
			// Programmer inputs.
			Numeric::SizedInt<16> pitch;
			Numeric::SizedInt<12> pulse_width;

			// State.
			Numeric::SizedInt<24> phase;
			Numeric::SizedInt<24> previous_phase;
		} oscillator;
		struct ADSR {
			// Programmer inputs.
			Numeric::SizedInt<4> attack;
			Numeric::SizedInt<4> decay;
			Numeric::SizedInt<4> sustain;
			Numeric::SizedInt<4> release;

			// State.
			Numeric::SizedInt<8> envelope;
			Numeric::SizedInt<15> rate_counter;
			enum class Phase {
				Attack,
				DecayAndHold,
				Release,
			} phase_ = Phase::Attack;
		} adsr;
		Numeric::SizedInt<8> control;

		bool noise() const		{ return control.bit<7>(); }
		bool pulse() const		{ return control.bit<6>(); }
		bool sawtooth() const	{ return control.bit<5>(); }
		bool triangle() const	{ return control.bit<4>(); }
		bool test() const		{ return control.bit<3>(); }	// Implemented.
		bool ring_mod() const	{ return control.bit<2>(); }
		bool sync() const		{ return control.bit<1>(); }	// Implemented.
		bool gate() const		{ return control.bit<0>(); }

		void update() {
			// Oscillator.
			oscillator.previous_phase = oscillator.phase;
			if(test()) {
				oscillator.phase = 0;
			} else {
				oscillator.phase += oscillator.pitch.get();
			}

			// TODO: ADSR.
		}

		void synchronise(const Voice &prior) {
			// Only oscillator work to do here.
			if(
				sync() &&
				!prior.oscillator.previous_phase.bit<23>() &&
				prior.oscillator.phase.bit<23>()
			) {
				oscillator.phase = 0;
			}
		}

		static constexpr uint16_t MaxWaveformValue = (1 << 12) - 1;

		uint16_t pulse_output() const {
			return oscillator.phase.get<12>() > oscillator.pulse_width.get() ? MaxWaveformValue : 0;
		}

		uint16_t output() const {
			// TODO: true composite waves.
			//
			// My current understanding on this: if multiple waveforms are enabled, the pull to zero beats the
			// pull to one on any line where the two compete. But the twist is that the lines are not necessarily
			// one per bit since they lead to a common ground. Ummm, I think.
			//
			// Anyway, first pass: logical AND. It's not right. It will temporarily do.

			uint16_t output = MaxWaveformValue;

			if(pulse()) output &= pulse_output();

			return output;
		}

	};

	Voice voices_[3];
};

}
