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

#include <cstdint>

namespace Commodore {
namespace Vic20 {

enum ROMSlot {
	Kernel = 0,
	BASIC,
	Characters,
	Drive
};

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

		/// Sets the contents of the rom in @c slot to the buffer @c data of length @c length.
		virtual void set_rom(ROMSlot slot, const std::vector<uint8_t> &data) = 0;

		/// Sets the memory size of this Vic-20.
		virtual void set_memory_size(MemorySize size) = 0;

		/// Sets the region of this Vic-20.
		virtual void set_region(Region region) = 0;

		/// Enables or disables turbo-speed tape loading.
		virtual void set_use_fast_tape_hack(bool activate) = 0;
};

}
}

#endif /* Vic20_hpp */
