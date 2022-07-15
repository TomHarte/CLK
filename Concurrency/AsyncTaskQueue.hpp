//
//  AsyncTaskQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef AsyncTaskQueue_hpp
#define AsyncTaskQueue_hpp

#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>
#include <vector>

#include "../ClockReceiver/TimeTypes.hpp"

#if defined(__APPLE__) && !defined(IGNORE_APPLE)
#include <dispatch/dispatch.h>
#define USE_GCD
#endif

namespace Concurrency {

/// An implementation detail; provides the time-centric part of a TaskQueue with a real Performer.
template <typename Performer> struct TaskQueueStorage {
	template <typename... Args> TaskQueueStorage(Args&&... args) :
		performer(std::forward<Args>(args)...),
		last_fired_(Time::nanos_now()) {}

	Performer performer;

	protected:
		void update() {
			auto time_now = Time::nanos_now();
			performer.perform(time_now - last_fired_);
			last_fired_ = time_now;
		}

	private:
		Time::Nanos last_fired_;
};

/// An implementation detail; provides a no-op implementation of time advances for TaskQueues without a Performer.
template <> struct TaskQueueStorage<void> {
	TaskQueueStorage() {}

	protected:
		void update() {}
};

/*!
	A task queue allows a caller to enqueue void(void) functions. Those functions are guaranteed
	to be performed serially and asynchronously from the caller.

	If @c perform_automatically is true, functions will be performed as soon as is possible,
	at the cost of thread synchronisation.

	If @c perform_automatically is false, functions will be queued up and not dispatched
	until a call to perform().

	If a @c Performer type is supplied then a public member, @c performer will be constructed
	with the arguments supplied to TaskQueue's constructor, and that class will receive  calls of the
	form @c .perform(nanos) to update it to every batch of new actions.
*/
template <bool perform_automatically, typename Performer = void> class TaskQueue: public TaskQueueStorage<Performer> {
	public:
		template <typename... Args> TaskQueue(Args&&... args) :
			TaskQueueStorage<Performer>(std::forward<Args>(args)...),
			thread_{
				[this] {
					ActionVector actions;

					while(!should_quit_) {
						// Wait for new actions to be signalled, and grab them.
						std::unique_lock lock(condition_mutex_);
						while(actions_.empty()) {
							condition_.wait(lock);
						}
						std::swap(actions, actions_);
						lock.unlock();

						// Update to now (which is possibly a no-op).
						TaskQueueStorage<Performer>::update();

						// Perform the actions and destroy them.
						for(const auto &action: actions) {
							action();
						}
						actions.clear();
					}
				}
			} {}

		/// Enqueus @c post_action to be performed asynchronously at some point
		/// in the future. If @c perform_automatically is @c true then the action
		/// will be performed as soon as possible. Otherwise it will sit unsheculed until
		/// a call to @c perform().
		///
		/// Actions may be elided.
		///
		/// If this TaskQueue has a @c Performer then the action will be performed
		/// on the same thread as the performer, after the performer has been updated
		/// to 'now'.
		void enqueue(const std::function<void(void)> &post_action) {
			std::lock_guard guard(condition_mutex_);
			actions_.push_back(post_action);

			if constexpr (perform_automatically) {
				condition_.notify_all();
			}
		}

		/// Causes any enqueued actions that are not yet scheduled to be scheduled.
		void perform() {
			if(actions_.empty()) {
				return;
			}
			condition_.notify_all();
		}

		/// Permanently stops this task queue, blocking until that has happened.
		/// All pending actions will be performed first.
		///
		/// The queue cannot be restarted; this is a destructive action.
		void stop() {
			if(thread_.joinable()) {
				should_quit_ = true;
				enqueue([] {});
				if constexpr (!perform_automatically) {
					perform();
				}
				thread_.join();
			}
		}

		/// Schedules any remaining unscheduled work, then blocks synchronously
		/// until all scheduled work has been performed.
		void flush() {
			std::mutex flush_mutex;
			std::condition_variable flush_condition;
			bool has_run = false;
			std::unique_lock lock(flush_mutex);

			enqueue([&flush_mutex, &flush_condition, &has_run] () {
				std::unique_lock inner_lock(flush_mutex);
				has_run = true;
				flush_condition.notify_all();
			});

			if constexpr (!perform_automatically) {
				perform();
			}

			flush_condition.wait(lock, [&has_run] { return has_run; });
		}

		~TaskQueue() {
			stop();
		}

	private:
		// The list of actions waiting be performed. These will be elided,
		// increasing their latency, if the emulation thread falls behind.
		using ActionVector = std::vector<std::function<void(void)>>;
		ActionVector actions_;

		// Necessary synchronisation parts.
		std::atomic<bool> should_quit_ = false;
		std::mutex condition_mutex_;
		std::condition_variable condition_;

		// Ensure the thread isn't constructed until after the mutex
		// and condition variable.
		std::thread thread_;
};

}

#endif /* AsyncTaskQueue_hpp */
