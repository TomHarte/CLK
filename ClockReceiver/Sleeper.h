//
//  Sleeper.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Sleeper_h
#define Sleeper_h

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
				void set_component_is_sleeping(void *component, bool is_sleeping) = 0;
		};

		/// Registers @c observer as the new sleep observer;
		void set_sleep_observer(SleepObserver *observer) {
			sleep_observer_ = delegate;
		}

	protected:
		/// Provided for subclasses; send sleep announcements to the sleep_observer_.
		SleepObserver *sleep_observer_;
};

#endif /* Sleeper_h */
