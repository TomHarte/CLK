//
//  OPL2.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef OPL2_hpp
#define OPL2_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Yamaha {

/*!
	Provides an emulation of the OPL2 core, along with its OPLL and VRC7 specialisations.
*/
class OPL2: public ::Outputs::Speaker::SampleSource {
	public:
		enum class Personality {
			OPL2,	// Provides full configuration of all channels.
			OPLL,	// Uses the OPLL sound font, permitting full configuration of only a single channel.
			VRC7,	// Uses the VRC7 sound font, permitting full configuration of only a single channel.
		};

		// Creates a new OPL2, OPLL or VRC7.
		OPL2(Personality personality, Concurrency::DeferringAsyncTaskQueue &task_queue);

		/// As per ::SampleSource; provides a broadphase test for silence.
		bool is_zero_level();

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);

		/// Writes to the OPL.
		void write(uint16_t address, uint8_t value);

		/// Reads from the OPL.
		uint8_t read(uint16_t address);

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;

};

}

#endif /* OPL2_hpp */
