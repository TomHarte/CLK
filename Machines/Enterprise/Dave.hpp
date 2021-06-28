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
		Audio(Concurrency::DeferringAsyncTaskQueue &audio_queue);

		void write(uint16_t address, uint8_t value);

		// MARK: - SampleSource.
		void set_sample_volume_range(int16_t range);
		static constexpr bool get_is_stereo() { return true; }	// Dave produces stereo sound.
		void get_samples(std::size_t number_of_samples, int16_t *target);

	private:
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;

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
			int output = false;
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
		void write(uint16_t address, uint8_t value);

		uint8_t get_new_interrupts();
		uint8_t get_divider_state();

		void run_for(Cycles);

		Cycles get_next_sequence_point() const;

	private:
		uint8_t interrupts_ = 0;

		static constexpr Cycles clock_rate{250000};
		static constexpr Cycles half_clock_rate{125000};

		Cycles one_hz_offset_ = clock_rate;

		int programmable_offset_ = 0;
		bool programmable_level_ = false;

		enum class InterruptRate {
			OnekHz,
			FiftyHz,
			ToneGenerator0,
			ToneGenerator1,
		} rate_ = InterruptRate::OnekHz;

		struct Channel {
			uint16_t value = 100, reload = 100;
			bool sync = false;
		} channels_[2];
};

}
}

#endif /* Dave_hpp */
