//
//  AsyncUpdater.h
//  Clock Signal
//
//  Created by Thomas Harte on 06/07/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef AsyncUpdater_hpp
#define AsyncUpdater_hpp

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "../ClockReceiver/TimeTypes.hpp"

namespace Concurrency {

template <typename Performer> class AsyncUpdater {
	public:
		template <typename... Args> AsyncUpdater(Args&&... args)
			: performer_(std::forward<Args>(args)...),
			performer_thread_{
				[this] {
					Time::Nanos last_fired = Time::nanos_now();

					while(!should_quit) {
						// Wait for a new action to be signalled, and grab it.
						std::unique_lock lock(condition_mutex_);
						while(actions_.empty()) {
							condition_.wait(lock);
						}
						auto action = actions_.pop_back();
						lock.unlock();

						// Update to now.
						auto time_now = Time::nanos_now();
						performer_.perform(time_now - last_fired);
						last_fired = time_now;

						// Perform the action.
						action();
					}
				}
			} {}

		/// Run the performer up to 'now' and then perform @c post_action.
		///
		/// @c post_action will be performed asynchronously, on the same
		/// thread as the performer.
		void update(const std::function<void(void)> &post_action) {
			std::lock_guard guard(condition_mutex_);
			actions_.push_back(post_action);
			condition_.notify_all();
		}

		~AsyncUpdater() {
			should_quit = true;
			update([] {});
			performer_thread_.join();
		}

	private:
		Performer performer_;

		std::thread performer_thread_;
		std::mutex condition_mutex_;
		std::condition_variable condition_;
		std::vector<std::function<void(void)>> actions_;
		std::atomic<bool> should_quit = false;
};


}

#endif /* AsyncUpdater_hpp */
