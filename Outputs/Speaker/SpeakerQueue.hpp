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

template <typename SpeakerT, typename GeneratorT>
struct SpeakerQueue: private Concurrency::EnqueueDelegate {
private:
	TaskQueue queue_;

public:
	SpeakerQueue(const Cycles divider) : generator(queue_), speaker(generator), divider_(divider) {
		queue_.set_enqueue_delegate(this);
	}

	GeneratorT generator;
	SpeakerT speaker;

	void operator += (const Cycles &duration) {
		cycles_since_update_ += duration;
	}

	void stop() {
		queue_.stop();
	}

	void perform() {
		// TODO: is there a way to avoid the empty lambda?
		queue_.enqueue([]() {});
		queue_.perform();
	}

private:
	Cycles divider_;
	Cycles cycles_since_update_;

	std::function<void(void)> prepare_enqueue() {
		return speaker.update_for(cycles_since_update_.divide(divider_));
	}
};
}
