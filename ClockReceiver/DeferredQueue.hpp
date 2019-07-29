//
//  DeferredQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef DeferredQueue_h
#define DeferredQueue_h

#include <functional>
#include <vector>

/*!
	A DeferredQueue maintains a list of ordered actions and the times at which
	they should happen, and divides a total execution period up into the portions
	that occur between those actions, triggering each action when it is reached.
*/
template <typename TimeUnit> class DeferredQueue {
	public:
		/// Constructs a DeferredQueue that will call target(period) in between deferred actions.
		DeferredQueue(std::function<void(TimeUnit)> &&target) : target_(std::move(target)) {}

		/*!
			Schedules @c action to occur in @c delay units of time.

			Actions must be scheduled in the order they will occur. It is undefined behaviour
			to schedule them out of order.
		*/
		void defer(TimeUnit delay, const std::function<void(void)> &action) {
			pending_actions_.emplace_back(delay, action);
		}

		/*!
			Runs for @c length units of time.

			The constructor-supplied target will be called with one or more periods that add up to @c length;
			any scheduled actions will be called between periods.
		*/
		void run_for(TimeUnit length) {
			// If there are no pending actions, just run for the entire length.
			// This should be the normal branch.
			if(pending_actions_.empty()) {
				target_(length);
				return;
			}

			// Divide the time to run according to the pending actions.
			while(length > TimeUnit(0)) {
				TimeUnit next_period = pending_actions_.empty() ? length : std::min(length, pending_actions_[0].delay);
				target_(next_period);
				length -= next_period;

				off_t performances = 0;
				for(auto &action: pending_actions_) {
					action.delay -= next_period;
					if(!action.delay) {
						action.action();
						++performances;
					}
				}
				if(performances) {
					pending_actions_.erase(pending_actions_.begin(), pending_actions_.begin() + performances);
				}
			}
		}

	private:
		std::function<void(TimeUnit)> target_;

		// The list of deferred actions.
		struct DeferredAction {
			TimeUnit delay;
			std::function<void(void)> action;

			DeferredAction(TimeUnit delay, const std::function<void(void)> &action) : delay(delay), action(std::move(action)) {}
		};
		std::vector<DeferredAction> pending_actions_;
};

#endif /* DeferredQueue_h */
