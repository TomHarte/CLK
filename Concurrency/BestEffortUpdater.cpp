//
//  BestEffortUpdater.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "BestEffortUpdater.hpp"

#include <cmath>

using namespace Concurrency;

void BestEffortUpdater::update() {
	if(!update_is_ongoing_.test_and_set()) {
		async_task_queue_.enqueue([this]() {
			std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
			auto elapsed = now - previous_time_point_;
			previous_time_point_ = now;

			if(has_previous_time_point_) {
				int64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
				double cycles = ((timestamp * clock_rate_) / 1e9) + error_;
				error_ = fmod(cycles, 1.0);

				if(delegate_) {
					delegate_->update(this, (int)cycles, has_skipped_);
				}
				has_skipped_ = false;
			} else {
				has_previous_time_point_ = true;
			}

			update_is_ongoing_.clear();
		});
	} else {
		async_task_queue_.enqueue([this]() {
			has_skipped_ = true;
		});
	}
}

void BestEffortUpdater::flush() {
	async_task_queue_.flush();
}

void BestEffortUpdater::set_delegate(Delegate *delegate) {
	async_task_queue_.enqueue([this, delegate]() {
		delegate_ = delegate;
	});
}

void BestEffortUpdater::set_clock_rate(double clock_rate) {
	async_task_queue_.enqueue([this, clock_rate]() {
		this->clock_rate_ = clock_rate;
	});
}
