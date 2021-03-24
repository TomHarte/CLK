//
//  ClockingHintSource.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ClockingHintSource_hpp
#define ClockingHintSource_hpp

namespace ClockingHint {

enum class Preference {
	/// The component doesn't currently require a clock signal.
	None,
	/// The component can be clocked only immediate prior to (explicit) accesses.
	JustInTime,
	/// The component require real-time clocking.
	RealTime
};

class Source;

struct Observer {
	/// Called to inform an observer that the component @c component has changed its clocking requirements.
	virtual void set_component_prefers_clocking(Source *component, Preference clocking) = 0;
};

/*!
	An clocking hint source is any component that can provide hints as to the type of
	clocking required for accurate emulation. A disk controller is an archetypal example.

	Types of clocking are:

		- none:
			a component that acts and reacts to direct contact but does not have a state that autonomously evolves.
			E.g. a ROM, RAM, or some kinds of disk controller when not in the process of performing a command.

		- just-in-time:
			a component that has an evolving state but can receive clock updates only immediately before a
			direct contact. This is possibly the most common kind of component.

		- real-time:
			a component that needs to be clocked in 'real time' (i.e. in terms of the emulated machine). For example
			so that it can announce an interrupt at the proper moment, because it is monitoring some aspect of
			the machine rather than waiting to be called upon, or because there's some other non-obvious relationship
			at play.

	A clocking hint source can signal changes in preferred clocking to an observer.

	This is intended to allow for performance improvements to machines with components that can be messaged selectively.
	The observer callout is virtual so the intended use case is that a machine holds a component that might go through
	periods of different clocking requirements.

	Transitions should be sufficiently infrequent that a virtual call to announce them costs little enough that
	the saved or deferred ::run_fors add up to a substantial amount.

	The hint provided is just that: a hint. Owners may perform ::run_for at a greater frequency.
*/
class Source {
	public:
		/// Registers @c observer as the new clocking observer.
		void set_clocking_hint_observer(Observer *observer) {
			observer_ = observer;
			update_clocking_observer();
		}

		/// @returns the current preferred clocking strategy.
		virtual Preference preferred_clocking() const = 0;

	private:
		Observer *observer_ = nullptr;

	protected:
		/*!
			Provided for subclasses; call this whenever the clocking preference might have changed.
			This will notify the observer if there is one.
		*/
		void update_clocking_observer() {
			if(!observer_) return;
			observer_->set_component_prefers_clocking(this, preferred_clocking());
		}
};

}

#endif /* ClockingHintSource_h */
