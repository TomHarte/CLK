//
//  Dave.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Dave_hpp
#define Dave_hpp

#include <cstdint>

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"
#include "../../Numeric/LFSR.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace Enterprise {
namespace Dave {

enum class Interrupt: uint8_t {
	VariableFrequency = 0x02,
	OneHz = 0x08,
	Nick = 0x20,
};

/*!
	Models the audio-production subset of Dave's behaviour.
*/
class Audio: public Outputs::Speaker::SampleSource {
	public:
		Audio(Concurrency::TaskQueue<false> &audio_queue);

		/// Modifies an register in the audio range; only the low 4 bits are
		/// used for register decoding so it's assumed that the caller has
		/// already identified this write as being to an audio register.
		void write(uint16_t address, uint8_t value);

		// MARK: - SampleSource.
		void set_sample_volume_range(int16_t range);
		static constexpr bool get_is_stereo() { return true; }	// Dave produces stereo sound.
		void get_samples(std::size_t number_of_samples, int16_t *target);

	private:
		Concurrency::TaskQueue<false> &audio_queue_;

		// Global divider (i.e. 8MHz/12Mhz switch).
		uint8_t global_divider_;
		uint8_t global_divider_reload_ = 2;

		// Tone channels.
		struct Channel {
			// User-set values.
			uint16_t reload = 0;
			bool high_pass = false;
			bool ring_modulate = false;
			enum class Distortion {
				None = 0,
				FourBit = 1,
				FiveBit = 2,
				SevenBit = 3,
			} distortion = Distortion::None;
			uint8_t amplitude[2]{};
			bool sync = false;

			// Current state.
			uint16_t count = 0;
			int output = 0;
		} channels_[3];
		void update_channel(int);

		// Noise channel.
		struct Noise {
			// User-set values.
			uint8_t amplitude[2]{};
			enum class Frequency {
				DivideByFour,
				ToneChannel0,
				ToneChannel1,
				ToneChannel2,
			} frequency = Frequency::DivideByFour;
			enum class Polynomial {
				SeventeenBit,
				FifteenBit,
				ElevenBit,
				NineBit
			} polynomial = Polynomial::SeventeenBit;
			bool swap_polynomial = false;
			bool low_pass = false;
			bool high_pass = false;
			bool ring_modulate = false;

			// Current state.
			int count = 0;
			int output = 0;
			bool final_output = false;
		} noise_;
		void update_noise();

		bool use_direct_output_[2]{};

		// Global volume, per SampleSource obligations.
		int16_t volume_ = 0;

		// Polynomials that are always running.
		Numeric::LFSRv<0xc> poly4_;
		Numeric::LFSRv<0x14> poly5_;
		Numeric::LFSRv<0x60> poly7_;

		// The selectable, noise-related polynomial.
		Numeric::LFSRv<0x110> poly9_;
		Numeric::LFSRv<0x500> poly11_;
		Numeric::LFSRv<0x6000> poly15_;
		Numeric::LFSRv<0x12000> poly17_;

		// Current state of the active polynomials.
		uint8_t poly_state_[4];
};

/*!
	Provides Dave's timed interrupts — those that are provided at 1 kHz,
	50 Hz or according to the rate of tone generators 0 or 1, plus the fixed
	1 Hz interrupt.
*/
class TimedInterruptSource {
	public:
		/// Modifies an register in the audio range; only the low 4 bits are
		/// used for register decoding so it's assumed that the caller has
		/// already identified this write as being to an audio register.
		void write(uint16_t address, uint8_t value);

		/// Returns a bitmask of interrupts that have become active since
		/// the last time this method was called; flags are as defined in
		/// @c Enterprise::Dave::Interrupt
		uint8_t get_new_interrupts();

		/// Returns the current high or low states of the inputs that trigger
		/// the interrupts modelled here, as a bit mask compatible with that
		/// exposed by Dave as the register at 0xb4.
		uint8_t get_divider_state();

		/// Advances the interrupt source.
		void run_for(Cycles);

		/// @returns The amount of time from now until the earliest that
		/// @c get_new_interrupts() _might_ have new interrupts to report.
		Cycles get_next_sequence_point() const;

	private:
		static constexpr Cycles clock_rate{250000};
		static constexpr Cycles half_clock_rate{125000};

		// Global divider (i.e. 8MHz/12Mhz switch).
		Cycles global_divider_ = Cycles(2);
		Cycles run_length_;

		// Interrupts that have fired since get_new_interrupts()
		// was last called.
		uint8_t interrupts_ = 0;

		// A counter for the 1Hz interrupt.
		int two_second_counter_ = 0;

		// A counter specific to the 1kHz and 50Hz timers, if in use.
		enum class InterruptRate {
			OnekHz,
			FiftyHz,
			ToneGenerator0,
			ToneGenerator1,
		} rate_ = InterruptRate::OnekHz;
		bool programmable_level_ = false;

		// A local duplicate of the counting state of the first two audio
		// channels, maintained in case either of those is used as an
		// interrupt source.
		struct Channel {
			int value = 100, reload = 100;
			bool sync = false;
			bool level = false;
		} channels_[2];
		void update_channel(int c, bool is_linked, int decrement);
};

}
}

#endif /* Dave_hpp */
