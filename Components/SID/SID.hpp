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
		void reset_phase() {
			phase = PhaseReload;
		}

		static constexpr uint32_t NoiseReload = 0x7'ffff;
		uint32_t noise = NoiseReload;

		bool did_raise_b23() const {
			return previous_phase > phase;
		}
		bool did_raise_b19() const {
			static constexpr int NoiseBit = 1 << (19 + 8);
			return (previous_phase ^ phase) & phase & NoiseBit;
		}
		uint16_t sawtooth_output() const {
			return (phase >> 20) ^ 0x800;
		}
		uint16_t noise_output() const {
			// Uses bits: 20, 18, 14, 11, 9, 5, 2 and 0, plus four more zero bits.
			return
				((noise >> 8) & 0b1000'0000'0000) |		// b20 -> b11
				((noise >> 8) & 0b0100'0000'0000) |		// b18 -> b10
				((noise >> 5) & 0b0010'0000'0000) |		// b14 -> b9
				((noise >> 3) & 0b0001'0000'0000) |		// b11 -> b8
				((noise >> 2) & 0b0000'1000'0000) |		// b9 -> b7
				((noise << 1) & 0b0000'0100'0000) |		// b5 -> b6
				((noise << 3) & 0b0000'0010'0000) |		// b2 -> b5
				((noise << 4) & 0b0000'0001'0000);		// b0 -> b4
		}
		void update_noise(const bool test) {
			noise =
				(noise << 1) |
				(((noise >> 17) ^ ((noise >> 22) | test)) & 1);
		}
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

		void set_phase(const Phase new_phase) {
			static constexpr uint16_t rate_prescaler[] = {
				9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720, 19532, 31251
			};
			static_assert(sizeof(rate_prescaler) / sizeof(*rate_prescaler) == 16);

			phase = new_phase;
			switch(phase) {
				case Phase::Attack:			rate_counter_target = rate_prescaler[attack.get()]; break;
				case Phase::DecayAndHold:	rate_counter_target = rate_prescaler[decay.get()]; break;
				case Phase::Release:		rate_counter_target = rate_prescaler[release.get()]; break;
			}
		}
	} adsr;
	Numeric::SizedInt<8> control;

	void set_control(const uint8_t new_control) {
		const bool old_gate = gate();
		control = new_control;
		if(gate() && !old_gate) {
			adsr.set_phase(ADSR::Phase::Attack);
		} else if(!gate() && old_gate) {
			adsr.set_phase(ADSR::Phase::Release);
		}
	}

	bool noise() const		{ return control.bit<7>(); }
	bool pulse() const		{ return control.bit<6>(); }
	bool sawtooth() const	{ return control.bit<5>(); }
	bool triangle() const	{ return control.bit<4>(); }
	bool test() const		{ return control.bit<3>(); }
	bool ring_mod() const	{ return control.bit<2>(); }
	bool sync() const		{ return control.bit<1>(); }
	bool gate() const		{ return control.bit<0>(); }

	void update() {
		// Oscillator.
		oscillator.previous_phase = oscillator.phase;
		if(test()) {
			oscillator.phase = 0;
		} else {
			oscillator.phase += oscillator.pitch;

			if(oscillator.did_raise_b19()) {
				oscillator.update_noise(test());
			}
		}

		// ADSR.

		// First prescalar, which is a function of the programmer-set rate.
		++ adsr.rate_counter;
		if(adsr.rate_counter == adsr.rate_counter_target) {
			adsr.rate_counter = 0;

			// Second prescalar, which approximates an exponential.
			static constexpr uint8_t exponential_prescaler[] = {
				1,														// 0
				30, 30, 30, 30, 30, 30,									// 1–6
				16, 16, 16, 16, 16, 16, 16, 16,							// 7–14
				8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,						// 15–26
				4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
				4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,				// 27–54
				2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,					// 55–94
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				1, 1, 1, 1, 1, 1, 1,
			};
			static_assert(sizeof(exponential_prescaler) == 256);
			static_assert(exponential_prescaler[0] == 1);
			static_assert(exponential_prescaler[1] == 30);
			static_assert(exponential_prescaler[6] == 30);
			static_assert(exponential_prescaler[7] == 16);
			static_assert(exponential_prescaler[14] == 16);
			static_assert(exponential_prescaler[15] == 8);
			static_assert(exponential_prescaler[26] == 8);
			static_assert(exponential_prescaler[27] == 4);
			static_assert(exponential_prescaler[54] == 4);
			static_assert(exponential_prescaler[55] == 2);
			static_assert(exponential_prescaler[94] == 2);
			static_assert(exponential_prescaler[95] == 1);
			static_assert(exponential_prescaler[255] == 1);

			if(adsr.phase == ADSR::Phase::Attack) {
				++adsr.envelope;
				// TODO: what really resets the exponential counter? If anything?
				adsr.exponential_counter = 0;
			} else {
				++adsr.exponential_counter;
				if(adsr.exponential_counter == exponential_prescaler[adsr.envelope]) {
					adsr.exponential_counter = 0;

					switch(adsr.phase) {
						default: __builtin_unreachable();
						case ADSR::Phase::DecayAndHold:
							if(adsr.envelope == adsr.sustain) {
								break;
							}
							--adsr.envelope;
						break;
						case ADSR::Phase::Release:
							if(adsr.envelope) {
								--adsr.envelope;
							}
						break;
					}
				}
			}
		}
	}

	void synchronise(const Voice &prior) {
		// Only oscillator work to do here.
		if(
			sync() &&
			prior.oscillator.did_raise_b23()
		) {
			oscillator.phase = Oscillator::PhaseReload;
		}
	}

	static constexpr uint16_t MaxWaveformValue = (1 << 12) - 1;

	uint16_t sawtooth_output() const {
		return oscillator.sawtooth_output();
	}

	uint16_t pulse_output() const {
		// TODO: find a better test than this.
		return (
			(oscillator.phase ^ 0x8000'0000) <
			(oscillator.pulse_width ^ 0x8000'0000))
				? MaxWaveformValue : 0;
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

		if(pulse())		output &= pulse_output();
		if(sawtooth())	output &= sawtooth_output();
		if(triangle())	output &= triangle_output(prior);
		if(noise())		output &= noise_output();

		return (output * adsr.envelope) / 255;
	}
};

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
	Voice voices_[3];

	// TODO: an emulator thread copy of voices needs to be kept too, to do the digital stuff, as
	// the current output of voice 3 can be read. Probably best if the audio thread posts its most
	// recent copy atomically and the emulator thread just catches up from whatever it has? I don't
	// think that spinning on voice 3 is common.
	uint8_t last_write_;

	int16_t range_ = 0;
	uint8_t volume_ = 0;
};

}
