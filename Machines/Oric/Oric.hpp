//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../../Configurable/Configurable.hpp"
#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include <cstdint>
#include <vector>

namespace Oric {

enum ROM {
	BASIC10 = 0, BASIC11, Microdisc, Colour
};

/// @returns The options available for an Oric.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

/*!
	Models an Oric 1/Atmos with or without a Microdisc.
*/
class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine,
	public Configurable::Device {
	public:
		virtual ~Machine();

		/// Creates and returns an Oric.
		static Machine *Oric();

		/// Sets the contents of @c rom to @c data. Assumed to be a setup step; has no effect once a machine is running.
		virtual void set_rom(ROM rom, const std::vector<uint8_t> &data) = 0;

		/// Enables or disables turbo-speed tape loading.
		virtual void set_use_fast_tape_hack(bool activate) = 0;

		/// Sets the type of display the Oric is connected to.
		virtual void set_output_device(Outputs::CRT::OutputDevice output_device) = 0;
};

}
#endif /* Oric_hpp */
