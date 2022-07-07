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
		template <typename... Args> AsyncUpdater(Args&&... args) :
			performer(std::forward<Args>(args)...),
			actions_(std::make_unique<ActionVector>()),
			performer_thread_{
				[this] {
					Time::Nanos last_fired = Time::nanos_now();
					auto actions = std::make_unique<ActionVector>();

					while(!should_quit) {
						// Wait for new actions to be signalled, and grab them.
						std::unique_lock lock(condition_mutex_);
						while(actions_->empty()) {
							condition_.wait(lock);
						}
						std::swap(actions, actions_);
						lock.unlock();

						// Update to now.
						auto time_now = Time::nanos_now();
						performer.perform(time_now - last_fired);
						last_fired = time_now;

						// Perform the actions.
						for(const auto& action: *actions) {
							action();
						}
						actions->clear();
					}
				}
			} {}

		/// Run the performer up to 'now' and then perform @c post_action.
		///
		/// @c post_action will be performed asynchronously, on the same
		/// thread as the performer.
		///
		/// Actions may be elided,
		void update(const std::function<void(void)> &post_action) {
			std::lock_guard guard(condition_mutex_);
			actions_->push_back(post_action);
			condition_.notify_all();
		}

		~AsyncUpdater() {
			should_quit = true;
			update([] {});
			performer_thread_.join();
		}

		// The object that will actually receive time advances.
		Performer performer;

	private:
		// The list of actions waiting be performed. These will be elided,
		// increasing their latency, if the emulation thread falls behind.
		using ActionVector = std::vector<std::function<void(void)>>;
		std::unique_ptr<ActionVector> actions_;

		// Necessary synchronisation parts.
		std::thread performer_thread_;
		std::mutex condition_mutex_;
		std::condition_variable condition_;
		std::atomic<bool> should_quit = false;
};


}

#endif /* AsyncUpdater_hpp */
