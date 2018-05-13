//
//  Sleeper.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Sleeper_hpp
#define Sleeper_hpp

/*!
	A sleeper is any component that sometimes requires a clock but at other times is 'asleep' — i.e. is not doing
	any clock-derived work, so needn't receive a clock. A disk controller is an archetypal example.

	A sleeper will signal sleeps and wakes to an observer.

	This is intended to allow for performance improvements to machines with components that can sleep. The observer
	callout is virtual so the intended use case is that a machine holds a component that might sleep. Its transitions
	into and out of sleep are sufficiently infrequent that a virtual call to announce them costs sufficiently little that
	the saved ::run_fors add up to a substantial amount.

	By convention, sleeper components must be willing to accept ::run_for even after announcing sleep. It's a hint,
	not a command.
*/
class Sleeper {
	public:
		Sleeper() : sleep_observer_(nullptr) {}

		class SleepObserver {
			public:
				/// Called to inform an observer that the component @c component has either gone to sleep or become awake.
				virtual void set_component_is_sleeping(Sleeper *component, bool is_sleeping) = 0;
		};

		/// Registers @c observer as the new sleep observer;
		void set_sleep_observer(SleepObserver *observer) {
			sleep_observer_ = observer;
		}

		/// @returns @c true if the component is currently sleeping; @c false otherwise.
		virtual bool is_sleeping() = 0;

	protected:
		/// Provided for subclasses; send sleep announcements to the sleep_observer_.
		SleepObserver *sleep_observer_;

		/*!
			Provided for subclasses; call this whenever is_sleeping might have changed, and the observer will be notified,
			if one exists.

			@c is_sleeping will be called only if there is an observer.
		*/
		void update_sleep_observer() {
			if(!sleep_observer_) return;
			sleep_observer_->set_component_is_sleeping(this, is_sleeping());
		}
};

#endif /* Sleeper_h */
