//
//  DeferredQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include <concepts>
#include <functional>
#include <vector>

/*!
	Provides the logic to insert into and traverse a list of future scheduled items.
*/
template <typename TimeUnit> class DeferredQueue {
public:
	/*!
		Schedules @c action to occur in @c delay units of time.
	*/
	template <typename FuncT>
	requires std::invocable<FuncT>
	void defer(TimeUnit delay, FuncT &&action) {
		// Apply immediately if there's no delay (or a negative delay).
		if(delay <= TimeUnit(0)) {
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

			pending_actions_.emplace(insertion_point, delay, std::forward<FuncT>(action));
		} else {
			pending_actions_.emplace_back(delay, std::forward<FuncT>(action));
		}
	}

	/*!
		@returns The amount of time until the next enqueued action will occur,
			or TimeUnit(-1) if the queue is empty.
	*/
	TimeUnit time_until_next_action() const {
		if(pending_actions_.empty()) return TimeUnit(-1);
		return pending_actions_.front().delay;
	}

	/*!
		Advances the queue the specified amount of time, performing any actions it reaches.
	*/
	void advance(const TimeUnit time) {
		auto remaining_time = time;
		auto erase_iterator = pending_actions_.begin();
		while(erase_iterator != pending_actions_.end()) {
			erase_iterator->delay -= remaining_time;
			if(erase_iterator->delay <= TimeUnit(0)) {
				remaining_time = -erase_iterator->delay;
				erase_iterator->action();
				++erase_iterator;
			} else {
				break;
			}
		}
		if(erase_iterator != pending_actions_.begin()) {
			pending_actions_.erase(pending_actions_.begin(), erase_iterator);
		}
	}

	/// @returns @c true if no actions are enqueued; @c false otherwise.
	bool empty() const {
		return pending_actions_.empty();
	}

private:
	// The list of deferred actions.
	struct DeferredAction {
		TimeUnit delay;
		std::function<void(void)> action;

		template <typename FuncT>
		requires std::invocable<FuncT>
		DeferredAction(TimeUnit delay, FuncT &&action) :
			delay(delay), action(std::forward<FuncT>(action)) {}
	};
	std::vector<DeferredAction> pending_actions_;
};

/*!
	A DeferredQueue maintains a list of ordered actions and the times at which
	they should happen, and divides a total execution period up into the portions
	that occur between those actions, triggering each action when it is reached.

	This list is efficient only for short queues.
*/
template <typename TimeUnit> class DeferredQueuePerformer: public DeferredQueue<TimeUnit> {
public:
	/// Constructs a DeferredQueue that will call target(period) in between deferred actions.
	template <typename FuncT>
	requires std::invocable<FuncT, TimeUnit>
	constexpr DeferredQueuePerformer(FuncT &&target) : target_(std::forward<FuncT>(target)) {}

	/*!
		Runs for @c length units of time.

		The constructor-supplied target will be called with one or more periods that add up to @c length;
		any scheduled actions will be called between periods.
	*/
	void run_for(const TimeUnit length) {
		const auto update = [this](const TimeUnit period) {
			target_(period);
			DeferredQueue<TimeUnit>::advance(period);
		};

		auto length_remaining = length;
		while(true) {
			const auto time_to_next = DeferredQueue<TimeUnit>::time_until_next_action();
			if(time_to_next == TimeUnit(-1) || time_to_next > length_remaining) {
				if(length_remaining > TimeUnit(0)) {
					update(length_remaining);
				}
				break;
			}

			length_remaining -= time_to_next;
			update(time_to_next);
		}
	}

private:
	std::function<void(TimeUnit)> target_;
};
