//
//  KonamiSCC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef KonamiSCC_hpp
#define KonamiSCC_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Konami {

/*!
	Provides an emulation of Konami's Sound Creative Chip ('SCC').

	The SCC is a primitive wavetable synthesis chip, offering 32-sample tables,
	and five channels of output. The original SCC uses the same wave for channels
	four and five, the SCC+ supports different waves for the two channels.
*/
class SCC: public ::Outputs::Speaker::SampleSource {
	public:
		/// Creates a new SCC.
		SCC(Concurrency::DeferringAsyncTaskQueue &task_queue);

		/// As per ::SampleSource; provides a broadphase test for silence.
		bool is_silent();

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);

		/// Writes to the SCC.
		void write(uint16_t address, uint8_t value);

		/// Reads from the SCC.
		uint8_t read(uint16_t address);

	private:
		struct Channel {
			int period = 0;
			int amplitude = 0;
		} channels_[5];

		struct Wavetable {
			std::int16_t samples[32];
		} waves_[4];

		std::uint8_t channel_enable_ = 0;
		std::uint8_t test_register_ = 0;

		Concurrency::DeferringAsyncTaskQueue &task_queue_;
};

}

#endif /* KonamiSCC_hpp */
