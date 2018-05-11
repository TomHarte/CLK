//
//  Card.h
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Card_h
#define Card_h

#include "../../Processors/6502/6502.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Activity/Observer.hpp"

namespace AppleII {

class Card {
	public:
		/*! Advances time by @c cycles, of which @c stretches were stretched. */
		virtual void run_for(Cycles half_cycles, int stretches) {}

		/*! Performs a bus operation; the card is implicitly selected. */
		virtual void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) = 0;

		/*! Supplies a target for observers. */
		virtual void set_activity_observer(Activity::Observer *observer) {}
};

}

#endif /* Card_h */
