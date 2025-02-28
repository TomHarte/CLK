//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include "Atari2600Inputs.h"

#include <memory>

namespace Atari2600 {

/*!
	Models an Atari 2600.
*/
struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns an Atari 2600 on the heap.
	static std::unique_ptr<Machine> Atari2600(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	/// Sets the switch @c input to @c state.
	virtual void set_switch_is_enabled(Atari2600Switch, bool) = 0;

	/// Gets the state of switch @c input.
	virtual bool get_switch_is_enabled(Atari2600Switch) const = 0;

	// Presses or releases the reset button.
	virtual void set_reset_switch(bool) = 0;
};

}
