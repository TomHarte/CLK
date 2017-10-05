//
//  BestEffortUpdater.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef BestEffortUpdater_hpp
#define BestEffortUpdater_hpp

#include <atomic>
#include <chrono>

#include "AsyncTaskQueue.hpp"

namespace Concurrency {

/*!
	Accepts timing cues from multiple threads and ensures that a delegate receives calls to total
	a certain number of cycles per second, that those calls are strictly serialised, and that no
	backlog of calls accrues.

	No guarantees about the thread that the delegate will be called on are made.
*/
class BestEffortUpdater {
	public:
		/// A delegate receives timing cues.
		struct Delegate {
			virtual void update(BestEffortUpdater *updater, int cycles, bool did_skip_previous_update) = 0;
		};

		/// Sets the current delegate.
		void set_delegate(Delegate *);

		/// Sets the clock rate of the delegate.
		void set_clock_rate(double clock_rate);

		/*!
			If the delegate is not currently in the process of an `update` call, calls it now to catch up to the current time.
			The call is asynchronous; this method will return immediately.
		*/
		void update();

		/// Blocks until any ongoing update is complete.
		void flush();

	private:
		std::atomic_flag update_is_ongoing_;
		AsyncTaskQueue async_task_queue_;

		std::chrono::time_point<std::chrono::high_resolution_clock> previous_time_point_;
		bool has_previous_time_point_ = false;
		double error_ = 0.0;
		bool has_skipped_ = false;

		Delegate *delegate_ = nullptr;
		double clock_rate_ = 1.0;
};

}

#endif /* BestEffortUpdater_hpp */
