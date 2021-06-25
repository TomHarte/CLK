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

		struct Channel {
			// User-set values.
			uint16_t reload = 0;
			bool high_pass = false;
			bool ring_modulate = false;
			enum class Distortion {
				None,
				FourBit,
				FiveBit,
				SevenBit,
			} distortion = Distortion::None;
			uint8_t amplitude[2]{};

			// Current state.
			uint16_t count = 0;
			bool output = true;
		} channels_[3];
		int16_t volume_ = 0;

		// Various polynomials that contribute to audio generation.
		Numeric::LFSRv<0xc> poly4_;
		Numeric::LFSRv<0x14> poly5_;
		Numeric::LFSRv<0x60> poly7_;
		Numeric::LFSRv<0x110> poly9_;
		Numeric::LFSRv<0x500> poly11_;
		Numeric::LFSRv<0x6000> poly15_;
		Numeric::LFSRv<0x12000> poly17_;
};

}

#endif /* Dave_hpp */
