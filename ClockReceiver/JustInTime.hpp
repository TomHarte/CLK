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
#include "ClockingHintSource.hpp"
#include "ForceInline.hpp"

/*!
	A JustInTimeActor holds (i) an embedded object with a run_for method; and (ii) an amount
	of time since run_for was last called.

	Time can be added using the += operator. The -> operator can be used to access the
	embedded object. All time accumulated will be pushed to object before the pointer is returned.

	Machines that accumulate HalfCycle time but supply to a Cycle-counted device may supply a
	separate @c TargetTimeScale at template declaration.

	If the held object implements get_next_sequence_point() then it'll be used to flush implicitly
	as and when sequence points are hit. Callers can use will_flush() to predict these.

	If the held object is a subclass of ClockingHint::Source, this template will register as an
	observer and potentially stop clocking or stop delaying clocking until just-in-time references
	as directed.

	TODO: incorporate and codify AsyncJustInTimeActor.
*/
template <class T, class LocalTimeScale = HalfCycles, int multiplier = 1, int divider = 1> class JustInTimeActor:
	public ClockingHint::Observer {
	private:
		/*!
			A std::unique_ptr deleter which causes an update_sequence_point to occur on the actor supplied
			to it at construction if it implements get_next_sequence_point(). Otherwise destruction is a no-op.

			**Does not delete the object.**

			This is used by the -> operators below, which provide a unique pointer to the enclosed object and
			update their sequence points upon its destruction — i.e. after the caller has made whatever call
			or calls as were relevant to the enclosed object.
		*/
		class SequencePointAwareDeleter {
			public:
				explicit SequencePointAwareDeleter(JustInTimeActor<T, LocalTimeScale, multiplier, divider> *actor) noexcept
					: actor_(actor) {}

				forceinline void operator ()(const T *const) const {
					if constexpr (has_sequence_points<T>::value) {
						actor_->update_sequence_point();
					}
				}

			private:
				JustInTimeActor<T, LocalTimeScale, multiplier, divider> *const actor_;
		};

		// This block of SFINAE determines whether objects of type T accepts Cycles or HalfCycles.
		using HalfRunFor = void (T::*const)(HalfCycles);
		static uint8_t half_sig(...);
		static uint16_t half_sig(HalfRunFor);
		using TargetTimeScale =
			std::conditional_t<
				sizeof(half_sig(&T::run_for)) == sizeof(uint16_t),
				HalfCycles,
				Cycles>;

	public:
		/// Constructs a new JustInTimeActor using the same construction arguments as the included object.
		template<typename... Args> JustInTimeActor(Args&&... args) : object_(std::forward<Args>(args)...) {
			if constexpr (std::is_base_of<ClockingHint::Source, T>::value) {
				object_.set_clocking_hint_observer(this);
			}
		}

		/// Adds time to the actor.
		///
		/// @returns @c true if adding time caused a flush; @c false otherwise.
		forceinline bool operator += (LocalTimeScale rhs) {
			if constexpr (std::is_base_of<ClockingHint::Source, T>::value) {
				if(clocking_preference_ == ClockingHint::Preference::None) {
					return false;
				}
			}

			if constexpr (multiplier != 1) {
				time_since_update_ += rhs * multiplier;
			} else {
				time_since_update_ += rhs;
			}
			is_flushed_ = false;

			if constexpr (std::is_base_of<ClockingHint::Source, T>::value) {
				if (clocking_preference_ == ClockingHint::Preference::RealTime) {
					flush();
					return true;
				}
			}

			if constexpr (has_sequence_points<T>::value) {
				time_until_event_ -= rhs * multiplier;
				if(time_until_event_ <= LocalTimeScale(0)) {
					time_overrun_ = time_until_event_ / divider;
					flush();
					update_sequence_point();
					return true;
				}
			}

			return false;
		}

		/// Flushes all accumulated time and returns a pointer to the included object.
		///
		/// If this object provides sequence points, checks for changes to the next
		/// sequence point upon deletion of the pointer.
		[[nodiscard]] forceinline auto operator->() {
			flush();
			return std::unique_ptr<T, SequencePointAwareDeleter>(&object_, SequencePointAwareDeleter(this));
		}

		/// Acts exactly as per the standard ->, but preserves constness.
		///
		/// Despite being const, this will flush the object and, if relevant, update the next sequence point.
		[[nodiscard]] forceinline auto operator -> () const {
			auto non_const_this = const_cast<JustInTimeActor<T, LocalTimeScale, multiplier, divider> *>(this);
			non_const_this->flush();
			return std::unique_ptr<const T, SequencePointAwareDeleter>(&object_, SequencePointAwareDeleter(non_const_this));
		}

		/// @returns a pointer to the included object, without flushing time.
		[[nodiscard]] forceinline T *last_valid() {
			return &object_;
		}

		/// @returns a const pointer to the included object, without flushing time.
		[[nodiscard]] forceinline const T *last_valid() const {
			return &object_;
		}

		/// @returns the amount of time since the object was last flushed, in the target time scale.
		[[nodiscard]] forceinline TargetTimeScale time_since_flush() const {
			if constexpr (divider == 1) {
				return time_since_update_;
			}
			return TargetTimeScale(time_since_update_.as_integral() / divider);
		}

		/// @returns the amount of time since the object was last flushed, plus the local time scale @c offset,
		/// converted to the target time scale.
		[[nodiscard]] forceinline TargetTimeScale time_since_flush(LocalTimeScale offset) const {
			if constexpr (divider == 1) {
				return time_since_update_ + offset;
			}
			return TargetTimeScale((time_since_update_ + offset).as_integral() / divider);
		}

		/// Flushes all accumulated time.
		///
		/// This does not affect this actor's record of when the next sequence point will occur.
		forceinline void flush() {
			if(!is_flushed_) {
				did_flush_ = is_flushed_ = true;
				if constexpr (divider == 1) {
					const auto duration = time_since_update_.template flush<TargetTimeScale>();
					object_.run_for(duration);
				} else {
					const auto duration = time_since_update_.template divide<TargetTimeScale>(LocalTimeScale(divider));
					if(duration > TargetTimeScale(0))
						object_.run_for(duration);
				}
			}
		}

		/// Indicates whether a flush has occurred since the last call to did_flush().
		[[nodiscard]] forceinline bool did_flush() {
			const bool did_flush = did_flush_;
			did_flush_ = false;
			return did_flush;
		}

		/// @returns a number in the range [-max, 0] indicating the offset of the most recent sequence
		/// point from the final time at the end of the += that triggered the sequence point.
		[[nodiscard]] forceinline LocalTimeScale last_sequence_point_overrun() {
			return time_overrun_;
		}

		/// @returns the number of cycles until the next sequence-point-based flush, if the embedded object
		/// supports sequence points; @c LocalTimeScale() otherwise.
		[[nodiscard]] LocalTimeScale cycles_until_implicit_flush() const {
			return time_until_event_ / divider;
		}

		/// Indicates whether a sequence-point-caused flush will occur if the specified period is added.
		[[nodiscard]] forceinline bool will_flush(LocalTimeScale rhs) const {
			if constexpr (!has_sequence_points<T>::value) {
				return false;
			}
			return rhs >= time_until_event_;
		}

		/// Indicates the amount of time, in the local time scale, until the first local slot that falls wholly
		/// after @c duration, if that delay were to occur in @c offset units of time from now.
		[[nodiscard]] forceinline LocalTimeScale back_map(TargetTimeScale duration, TargetTimeScale offset) const {
			// A 1:1 mapping is easy.
			if constexpr (multiplier == 1 && divider == 1) {
				return duration;
			}

			// Work out when this query is placed, and the time to which it relates
			const auto base = time_since_update_ + offset * divider;
			const auto target = base + duration * divider;

			// Figure out the number of whole input steps that is required to get
			// past target, and subtract the number of whole input steps necessary
			// to get to base.
			const auto steps_to_base = base.as_integral() / multiplier;
			const auto steps_to_target = (target.as_integral() + divider - 1) / multiplier;

			return LocalTimeScale(steps_to_target - steps_to_base);
		}

		/// Updates this template's record of the next sequence point.
		void update_sequence_point() {
			if constexpr (has_sequence_points<T>::value) {
				// Keep a fast path where no conversions will be applied; if conversions are
				// going to be applied then do a direct max -> max translation rather than
				// allowing the arithmetic to overflow.
				if constexpr (divider == 1 && std::is_same_v<LocalTimeScale, TargetTimeScale>) {
					time_until_event_ = object_.get_next_sequence_point();
				} else {
					const auto time = object_.get_next_sequence_point();
					if(time == TargetTimeScale::max()) {
						time_until_event_ = LocalTimeScale::max();
					} else {
						time_until_event_ = time * divider;
					}
				}
				assert(time_until_event_ > LocalTimeScale(0));
			}
		}

		/// @returns A cached copy of the object's clocking preference.
		ClockingHint::Preference clocking_preference() const {
			return clocking_preference_;
		}

	private:
		T object_;
		LocalTimeScale time_since_update_, time_until_event_, time_overrun_;
		bool is_flushed_ = true;
		bool did_flush_ = false;

		template <typename S, typename = void> struct has_sequence_points : std::false_type {};
		template <typename S> struct has_sequence_points<S, decltype(void(std::declval<S &>().get_next_sequence_point()))> : std::true_type {};

		ClockingHint::Preference clocking_preference_ = ClockingHint::Preference::JustInTime;
		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference clocking) {
			clocking_preference_ = clocking;
		}
};

/*!
	An AsyncJustInTimeActor acts like a JustInTimeActor but additionally contains an AsyncTaskQueue.
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
