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

#include "../../Concurrency/AsyncTaskQueue.hpp"
#include "../../Numeric/LFSR.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace Enterprise {

/*!
	Models a subset of Dave's behaviour; memory mapping and interrupt status
	is integrated into the main Enterprise machine.
*/
class Dave: public Outputs::Speaker::SampleSource {
	public:
		Dave(Concurrency::DeferringAsyncTaskQueue &audio_queue);

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

			// Current state.
			uint16_t count = 0;
			int output = 0;
		} channels_[3];

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
			bool output = false;
			uint16_t count = 0;
		} noise_;

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

}

#endif /* Dave_hpp */
