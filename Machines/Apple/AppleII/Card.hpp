//
//  Card.h
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Card_h
#define Card_h

#include "../../../Processors/6502/6502.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Activity/Observer.hpp"

namespace Apple {
namespace II {

/*!
	This provides a small subset of the interface offered to cards installed in
	an Apple II, oriented pragmatically around the cards that are implemented.

	The main underlying rule is as it is elsewhere in the emulator: no
	_inaccurate_ simplifications — no provision of information that shouldn't
	actually commute, no interfaces that say they do one thing but which by both
	both sides are coupled through an unwritten understanding of abuse.

	Special notes:

		Devices that announce a select constraint, being interested in acting only
		when their IO or Device select is active will receive just-in-time @c run_for
		notifications, as well as being updated at the end of each of the Apple's
		@c run_for periods, prior to a @c flush.

		Devices that do not announce a select constraint will prima facie receive a
		@c perform_bus_operation every cycle. They'll also receive a @c flush.
		It is **highly** recomended that such devices also implement @c Sleeper
		as they otherwise prima facie require a virtual method call every
		single cycle.
*/
class Card {
	public:
		virtual ~Card() {}
		enum Select: int {
			None		= 0,		// No select line is active.
			IO			= 1 << 0,	// IO select is active; i.e. access is in range $C0x0 to $C0xf.
			Device		= 1 << 1,	// Device select is active; i.e. access is in range $Cx00 to $Cxff.

			C8Region	= 1 << 2,	// Access is to the region $c800 to $cfff, was preceded by at least
									// one Device access to this card, and has not yet been followed up
									// by an access to $cfff.
		};

		/*!
			Advances time by @c cycles, of which @c stretches were stretched.

			This is posted only to cards that announced a select constraint. Cards with
			no constraints, that want to be informed of every machine cycle, will receive
			a call to perform_bus_operation every cycle and should use that for time keeping.
		*/
		virtual void run_for([[maybe_unused]] Cycles cycles, [[maybe_unused]] int stretches) {}

		/// Requests a flush of any pending audio or video output.
		virtual void flush() {}

		/*!
			Performs a bus operation.

			@param select The state of the card's select lines: indicates whether the Apple II
				thinks this card should respond as though this were an IO access, a Device access,
				or it thinks that the card shouldn't respond.
			@param is_read @c true if this is a read cycle; @c false otherwise.
			@param address The current value of the address bus.
			@param value A pointer to the value of the data bus, not accounting input from cards.
				If this is a read cycle, the card is permitted to replace this value with the value
				output by the card, if any. If this is a write cycle, the card should only read
				this value.
		*/
		virtual void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) = 0;

		/*!
			Returns the type of bus selects this card is actually interested in.
			As specified, the default is that cards will ask to receive perform_bus_operation
			only when their select lines are active.

			There's a substantial caveat here: cards that register to receive @c None
			will receive a perform_bus_operation every cycle. To reduce the number of
			virtual method calls, they **will not** receive run_for. run_for will propagate
			only to cards that register for IO and/or Device accesses only.
		*/
		int get_select_constraints() const {
			return select_constraints_;
		}

		/*! Cards may supply a target for activity observation if desired. */
		virtual void set_activity_observer([[maybe_unused]] Activity::Observer *observer) {}

		struct Delegate {
			virtual void card_did_change_select_constraints(Card *card) = 0;
		};
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

	protected:
		int select_constraints_ = IO | Device;
		Delegate *delegate_ = nullptr;
		void set_select_constraints(int constraints) {
			if(constraints == select_constraints_) return;
			select_constraints_ = constraints;
			if(delegate_) delegate_->card_did_change_select_constraints(this);
		}
};

}
}

#endif /* Card_h */
