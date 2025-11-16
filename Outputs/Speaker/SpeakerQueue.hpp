//
//  SpeakerQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Concurrency/AsyncTaskQueue.hpp"
#include "ClockReceiver/ClockReceiver.hpp"

namespace Outputs::Speaker {
using TaskQueue = Concurrency::AsyncTaskQueue<false, true, true>;

template <typename CyclesT, typename SpeakerT, typename GeneratorT>
struct SpeakerQueue: private Concurrency::EnqueueDelegate {
	constexpr SpeakerQueue(const CyclesT divider) noexcept :
		generator_(queue_), speaker_(generator_), divider_(divider)
	{
		queue_.set_enqueue_delegate(this);
	}

	constexpr SpeakerQueue(const float input_rate, const CyclesT divider, const float high_cutoff = -1.0f) noexcept :
		SpeakerQueue(divider)
	{
		speaker_.set_input_rate(input_rate);
		if(high_cutoff >= 0.0) {
			speaker_.set_high_frequency_cutoff(high_cutoff);
		}
	}

	void operator += (const CyclesT &duration) {
		time_since_update_ += duration;
	}

	void stop() {
		queue_.stop();
	}

	void perform() {
		// TODO: is there a way to avoid the empty lambda?
		queue_.enqueue([]() {});
		queue_.perform();
	}

	SpeakerT &speaker() {
		return speaker_;
	}

	GeneratorT *operator ->() {
		return &generator_;
	}

private:
	TaskQueue queue_;
	GeneratorT generator_;
	SpeakerT speaker_;
	CyclesT divider_;
	CyclesT time_since_update_;

	std::function<void(void)> prepare_enqueue() final {
		return speaker_.update_for(time_since_update_.template divide<Cycles>(divider_));
	}
};

}
