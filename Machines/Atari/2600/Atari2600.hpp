//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include "Atari2600Inputs.h"

namespace Atari2600 {

/*!
	Models an Atari 2600.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Atari 2600 on the heap.
		static Machine *Atari2600(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Sets the switch @c input to @c state.
		virtual void set_switch_is_enabled(Atari2600Switch input, bool state) = 0;

		/// Gets the state of switch @c input.
		virtual bool get_switch_is_enabled(Atari2600Switch input) = 0;

		// Presses or releases the reset button.
		virtual void set_reset_switch(bool state) = 0;
};

}

#endif /* Atari2600_cpp */
