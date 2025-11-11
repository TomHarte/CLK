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

			static constexpr uint32_t NoiseReload = 0x7'ffff;
			uint32_t noise = NoiseReload;

			bool did_rise_b23() const {
				return previous_phase > phase;
			}
			uint16_t sawtooth_output() const {
				return (phase >> 20) ^ 0x800;
			}
			uint16_t noise_output() const {
				// Uses bits: 20, 18, 14, 11, 9, 5, 2 and 0, plus four more zero bits.
				return
					((noise >> 8) & 0x800) |
					((noise >> 7) & 0x400) |
					((noise >> 4) & 0x200) |
					((noise >> 2) & 0x100) |
					((noise >> 1) & 0x080) |
					((noise << 2) & 0x040) |
					((noise << 4) & 0x020) |
					((noise << 5) & 0x010);
			}
			void update_noise() {
				noise =
					(noise << 1) |
					(((noise >> 17) ^ (noise >> 20)) & 1);
			}
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
		bool test() const		{ return control.bit<3>(); }
		bool ring_mod() const	{ return control.bit<2>(); }
		bool sync() const		{ return control.bit<1>(); }
		bool gate() const		{ return control.bit<0>(); }	// TODO

		void update() {
			// Oscillator.
			oscillator.previous_phase = oscillator.phase;
			if(test()) {
				oscillator.phase = 0;
			} else {
				oscillator.phase += oscillator.pitch;

				if(oscillator.did_rise_b23()) {
					oscillator.update_noise();
				}
			}

			// TODO: ADSR.
		}

		void synchronise(const Voice &prior) {
			// Only oscillator work to do here.
			if(
				sync() &&
				prior.oscillator.did_rise_b23()
			) {
				oscillator.phase = Oscillator::PhaseReload;
			}
		}

		static constexpr uint16_t MaxWaveformValue = (1 << 12) - 1;

		uint16_t sawtooth_output() const {
			return oscillator.sawtooth_output();
		}

		uint16_t pulse_output() const {
			return (oscillator.phase > oscillator.pulse_width) ? MaxWaveformValue : 0;
		}

		uint16_t triangle_output(const Voice &prior) const {
			const uint16_t sawtooth = oscillator.sawtooth_output();
			const uint16_t xor_mask1 = sawtooth;
			const uint16_t xor_mask2 = ring_mod() ? prior.sawtooth() : 0;
			const uint16_t xor_mask = ((xor_mask1 ^ xor_mask2) & 0x800) ? 0xfff : 0x000;
			return ((sawtooth << 1) ^ xor_mask) & 0xfff;
		}

		uint16_t noise_output() const {
			return oscillator.noise_output();
		}

		uint16_t output(const Voice &prior) const {
			// TODO: true composite waves.
			//
			// My current understanding on this: if multiple waveforms are enabled, the pull to zero beats the
			// pull to one on any line where the two compete. But the twist is that the lines are not necessarily
			// one per bit since they lead to a common ground. Ummm, I think.
			//
			// Anyway, first pass: logical AND. It's not right. It will temporarily do.

			uint16_t output = MaxWaveformValue;

			if(pulse())
				output &= pulse_output();
			if(sawtooth())
				output &= sawtooth_output();
			if(triangle())
				output &= triangle_output(prior);
			if(noise())
				output &= noise_output();

			return output;
		}

	};

	Voice voices_[3];
};

}
