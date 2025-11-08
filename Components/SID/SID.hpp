//
//  SID.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"

namespace MOS::SID {

class SID: public Outputs::Speaker::BufferSource<SID, false> {
public:
	SID(Concurrency::AsyncTaskQueue<false> &audio_queue);

	template <Outputs::Speaker::Action action>
		void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
	bool is_zero_level() const;
	void set_sample_volume_range(std::int16_t);

private:
	Concurrency::AsyncTaskQueue<false> &audio_queue_;
};

}
