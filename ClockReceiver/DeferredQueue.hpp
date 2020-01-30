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

	This list is efficient only for short queues.
*/
template <typename TimeUnit> class DeferredQueue {
	public:
		/// Constructs a DeferredQueue that will call target(period) in between deferred actions.
		DeferredQueue(std::function<void(TimeUnit)> &&target) : target_(std::move(target)) {}

		/*!
			Schedules @c action to occur in @c delay units of time.
		*/
		void defer(TimeUnit delay, const std::function<void(void)> &action) {
			// Apply immediately if there's no delay (or a negative delay).
			if(delay <= 0) {
				action();
				return;
			}

			if(!pending_actions_.empty()) {
				// Otherwise enqueue, having subtracted the delay for any preceding events,
				// and subtracting from the subsequent, if any.
				auto insertion_point = pending_actions_.begin();
				while(insertion_point != pending_actions_.end() && insertion_point->delay < delay) {
					delay -= insertion_point->delay;
					++insertion_point;
				}
				if(insertion_point != pending_actions_.end()) {
					insertion_point->delay -= delay;
				}

				pending_actions_.emplace(insertion_point, delay, action);
			} else {
				pending_actions_.emplace_back(delay, action);
			}
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
			auto erase_iterator = pending_actions_.begin();
			while(erase_iterator != pending_actions_.end()) {
				erase_iterator->delay -= length;
				if(erase_iterator->delay <= TimeUnit(0)) {
					target_(length + erase_iterator->delay);
					length = -erase_iterator->delay;
					erase_iterator->action();
					++erase_iterator;
				} else {
					break;
				}
			}
			if(erase_iterator != pending_actions_.begin()) {
				pending_actions_.erase(pending_actions_.begin(), erase_iterator);
			}
			if(length != TimeUnit(0)) {
				target_(length);
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
