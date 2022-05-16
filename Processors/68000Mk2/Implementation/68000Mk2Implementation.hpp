//
//  68000Mk2Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef _8000Mk2Implementation_h
#define _8000Mk2Implementation_h

namespace CPU {
namespace MC68000Mk2 {

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::run_for(HalfCycles duration) {
	// Accumulate the newly paid-in cycles. If this instance remains in deficit, exit.
	time_remaining += duration;
	if(time_remaining <= HalfCycles(0)) return;

	// Check whether all remaining time has been expended; if so then exit, having set this line up as
	// the next resumption point.
#define ConsiderExit()	if(time_remaining <= HalfCycles(0)) { state = __LINE__; return; } [[fallthrough]]; case __LINE__:

	// Subtracts `n` half-cycles from `time_remaining`; if permit_overrun is false, also ConsiderExit()
#define Spend(n)		cycles_owed -= HalfCycles(n); if constexpr (!permit_overrun) ConsiderExit()

	// Performs ConsiderExit() only if permit_overrun is true
#define CheckOverrun()	if constexpr (permit_overrun) ConsiderExit()

	//
	// So structure is, in general:
	//
	//	case Action:
	//		do_something();
	//		Spend(20);
	//		do_something_else();
	//		Spend(10);
	//		do_a_third_thing();
	//		Spend(30);
	//		CheckOverrun();
	//
	//		state = next_action;
	//		break;
	//
	// Additional notes:
	//
	//	Action and all equivalents should be negative values, since the
	//	switch-for-computed-goto-for-a-coroutine structure uses __LINE__ for
	//	its invented entry- and exit-points, meaning that negative numbers are
	//	the easiest group that is safely definitely never going to collide.

	// Otherwise continue for all time, until back in debt.
	// Formatting is slightly obtuse here to make this look more like a coroutine.
	while(true) { switch(state) {


	}}

#undef CheckOverrun
#undef Spend
#undef ConsiderExit

}

}
}

#endif /* _8000Mk2Implementation_h */
