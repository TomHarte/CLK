//
//  BestEffortUpdater.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef BestEffortUpdater_hpp
#define BestEffortUpdater_hpp

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../ClockReceiver/TimeTypes.hpp"

namespace Concurrency {

/*!
	Accepts timing cues from multiple threads and ensures that a delegate receives calls to total
	a certain number of cycles per second, that those calls are strictly serialised, and that no
	backlog of calls accrues.

	No guarantees about the thread that the delegate will be called on are made.
*/
class BestEffortUpdater {
	public:
		BestEffortUpdater();
		~BestEffortUpdater();

		/// A delegate receives timing cues.
		struct Delegate {
			virtual void update(BestEffortUpdater *updater, Time::Seconds duration, bool did_skip_previous_update, int flags) = 0;
		};

		/// Sets the current delegate.
		void set_delegate(Delegate *);

		/*!
			If the delegate is not currently in the process of an `update` call, calls it now to catch up to the current time.
			The call is asynchronous; this method will return immediately.
		*/
		void update();

		/// Blocks until any ongoing update is complete; may spin.
		void flush();

	private:
		std::atomic<bool> should_quit_;
		std::atomic<bool> is_updating_;

		std::chrono::time_point<std::chrono::high_resolution_clock> target_time_;
		bool update_requested_;
		std::mutex update_mutex_;
		std::condition_variable update_condition_;

		std::chrono::time_point<std::chrono::high_resolution_clock> previous_time_point_;
		bool has_previous_time_point_ = false;
		std::atomic<bool> has_skipped_ = false;

		std::atomic<Delegate *>delegate_ = nullptr;

		void update_loop();

		// This is deliberately at the bottom, to ensure it constructs after the various
		// mutexs, conditions, etc, that it'll depend upon.
		std::thread update_thread_;
};

}

#endif /* BestEffortUpdater_hpp */
