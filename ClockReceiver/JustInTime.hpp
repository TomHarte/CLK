//
//  JustInTime.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef JustInTime_h
#define JustInTime_h

#include "../Concurrency/AsyncTaskQueue.hpp"

/*!
	A JustInTimeActor holds (i) an embedded object with a run_for method; and (ii) an amount
	of time since run_for was last called.

	Time can be added using the += operator. The -> operator can be used to access the
	embedded object. All time accumulated will be pushed to object before the pointer is returned.

	Machines that accumulate HalfCycle time but supply to a Cycle-counted device may supply a
	separate @c TargetTimeScale at template declaration.
*/
template <class T, class LocalTimeScale = HalfCycles, class TargetTimeScale = LocalTimeScale> class JustInTimeActor {
	public:
		/// Constructs a new JustInTimeActor using the same construction arguments as the included object.
		template<typename... Args> JustInTimeActor(Args&&... args) : object_(std::forward<Args>(args)...) {}

		/// Adds time to the actor.
		inline void operator += (const LocalTimeScale &rhs) {
			time_since_update_ += rhs;
			is_flushed_ = false;
		}

		/// Flushes all accumulated time and returns a pointer to the included object.
		inline T *operator->() {
			flush();
			return &object_;
		}

		/// Returns a pointer to the included object without flushing time.
		inline T *last_valid() {
			return &object_;
		}

		/// Flushes all accumulated time.
		inline void flush() {
			if(!is_flushed_) object_.run_for(time_since_update_.template flush<TargetTimeScale>());
			is_flushed_ = true;
		}

	private:
		T object_;
		LocalTimeScale time_since_update_;
		bool is_flushed_ = true;
};

/*!
	A AsyncJustInTimeActor acts like a JustInTimeActor but additionally contains an AsyncTaskQueue.
	Any time the amount of accumulated time crosses a threshold provided at construction time,
	the object will be updated on the AsyncTaskQueue.
*/
template <class T, class LocalTimeScale = HalfCycles, class TargetTimeScale = LocalTimeScale> class AsyncJustInTimeActor {
	public:
		/// Constructs a new AsyncJustInTimeActor using the same construction arguments as the included object.
		template<typename... Args> AsyncJustInTimeActor(TargetTimeScale threshold, Args&&... args) :
			object_(std::forward<Args>(args)...),
		 	threshold_(threshold) {}

		/// Adds time to the actor.
		inline void operator += (const LocalTimeScale &rhs) {
			time_since_update_ += rhs;
			if(time_since_update_ >= threshold_) {
				time_since_update_ -= threshold_;
				task_queue_.enqueue([this] () {
					object_.run_for(threshold_);
				});
			}
			is_flushed_ = false;
		}

		/// Flushes all accumulated time and returns a pointer to the included object.
		inline T *operator->() {
			flush();
			return &object_;
		}

		/// Returns a pointer to the included object without flushing time.
		inline T *last_valid() {
			return &object_;
		}

		/// Flushes all accumulated time.
		inline void flush() {
			if(!is_flushed_) {
				task_queue_.flush();
				object_.run_for(time_since_update_.template flush<TargetTimeScale>());
				is_flushed_ = true;
			}
		}

	private:
		T object_;
		LocalTimeScale time_since_update_;
		TargetTimeScale threshold_;
		bool is_flushed_ = true;
		Concurrency::AsyncTaskQueue task_queue_;
};

#endif /* JustInTime_h */
