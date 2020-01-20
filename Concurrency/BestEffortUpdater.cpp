//
//  BestEffortUpdater.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "BestEffortUpdater.hpp"

#include <cmath>

using namespace Concurrency;

BestEffortUpdater::BestEffortUpdater() :
	update_thread_([this]() {
		this->update_loop();
	}) {}

BestEffortUpdater::~BestEffortUpdater() {
	// Sever the delegate now, as soon as possible, then wait for any
	// pending tasks to finish.
	set_delegate(nullptr);
	flush();

	// Wind up the update thread.
	should_quit_ = true;
	update();
	update_thread_.join();
}

void BestEffortUpdater::update() {
	// Bump the requested target time and set the update requested flag.
	{
		std::lock_guard<decltype(update_mutex_)> lock(update_mutex_);
		has_skipped_ = update_requested_;
		update_requested_ = true;
		target_time_ = std::chrono::high_resolution_clock::now();
	}
	update_condition_.notify_one();
}

void BestEffortUpdater::update_loop() {
 	while(true) {
		std::unique_lock<decltype(update_mutex_)> lock(update_mutex_);
		is_updating_ = false;

		// Wait to be signalled.
		update_condition_.wait(lock, [this]() -> bool {
			return update_requested_;
		});

		// Possibly this signalling really means 'quit'.
		if(should_quit_) return;

		// Note update started, crib the target time.
		auto target_time = target_time_;
		update_requested_ = false;

		// If this was actually the first update request, silently swallow it.
		if(!has_previous_time_point_) {
			has_previous_time_point_ = true;
			previous_time_point_ = target_time;
			continue;
		}

		// Release the lock on requesting new updates.
		is_updating_ = true;
		lock.unlock();

		// Calculate period from previous time to now.
		const auto elapsed = target_time - previous_time_point_;
		previous_time_point_ = target_time;

		// Invoke the delegate, if supplied, in order to run.
		const int64_t integer_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
		if(integer_duration > 0) {
			const auto delegate = delegate_.load();
			if(delegate) {
				// Cap running at 1/5th of a second, to avoid doing a huge amount of work after any
				// brief system interruption.
				const double duration = std::min(double(integer_duration) / 1e9, 0.2);
				delegate->update(this, duration, has_skipped_, 0);
				has_skipped_ = false;
			}
		}
	}
}

void BestEffortUpdater::flush() {
	// Spin lock; this is allowed to be slow.
	while(true) {
		std::lock_guard<decltype(update_mutex_)> lock(update_mutex_);
		if(!is_updating_) return;
	}
}

void BestEffortUpdater::set_delegate(Delegate *const delegate) {
	delegate_.store(delegate);
}

