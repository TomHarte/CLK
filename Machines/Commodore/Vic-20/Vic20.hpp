//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../ConfigurationTarget.hpp"
#include "../../CRTMachine.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../JoystickMachine.hpp"

namespace Commodore {
namespace Vic20 {

enum MemorySize {
	Default,
	ThreeKB,
	ThirtyTwoKB
};

enum Region {
	American,
	Danish,
	Japanese,
	European,
	Swedish
};

/// @returns The options available for a Vic-20.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine,
	public JoystickMachine::Machine,
	public Configurable::Device {
	public:
		virtual ~Machine();

		/// Creates and returns a Vic-20.
		static Machine *Vic20();

		/// Sets the memory size of this Vic-20.
		virtual void set_memory_size(MemorySize size) = 0;

		/// Sets the region of this Vic-20.
		virtual void set_region(Region region) = 0;
};

}
}

#endif /* Vic20_hpp */
