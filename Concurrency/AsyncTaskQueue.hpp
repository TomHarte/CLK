//
//  AsyncTaskQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>
#include <vector>

#include "../ClockReceiver/TimeTypes.hpp"

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
	A task queue allows a caller to enqueue @c void(void) functions. Those functions are guaranteed
	to be performed serially and asynchronously from the caller.

	If @c perform_automatically is true, functions will be performed as soon as is possible,
	at the cost of thread synchronisation.

	If @c perform_automatically is false, functions will be queued up but not dispatched
	until a call to perform().

	If a @c Performer type is supplied then a public member, @c performer will be constructed
	with the arguments supplied to TaskQueue's constructor. That instance will receive calls of the
	form @c .perform(nanos) before every batch of new actions, indicating how much time has
	passed since the previous @c perform.

	@note Even if @c perform_automatically is true, actions may be batched, when a long-running
	action occupies the asynchronous thread for long enough. So it is not true that @c perform will be
	called once per action.
*/
template <bool perform_automatically, bool start_immediately = true, typename Performer = void> class AsyncTaskQueue: public TaskQueueStorage<Performer> {
	public:
		template <typename... Args> AsyncTaskQueue(Args&&... args) :
			TaskQueueStorage<Performer>(std::forward<Args>(args)...) {
			if constexpr (start_immediately) {
				start();
			}
		}

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

		/// Starts the queue if it has never been started before.
		///
		/// This is not guaranteed safely to restart a stopped queue.
		void start() {
			thread_ = std::thread{
				[this] {
					ActionVector actions;

					// Continue until told to quit.
					while(!should_quit_) {
						// Wait for new actions to be signalled, and grab them.
						std::unique_lock lock(condition_mutex_);
						while(actions_.empty() && !should_quit_) {
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
			};
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

		~AsyncTaskQueue() {
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
