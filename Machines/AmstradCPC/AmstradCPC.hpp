//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include <cstdint>
#include <vector>

namespace AmstradCPC {

enum ROMType: int {
	OS464 = 0,	BASIC464,
	OS664,		BASIC664,
	OS6128,		BASIC6128,
	AMSDOS
};

/*!
	Models an Amstrad CPC.
*/
class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amstrad CPC.
		static Machine *AmstradCPC();

		/// Sets the contents of rom @c type to @c data. Assumed to be a setup step; has no effect once a machine is running.
		virtual void set_rom(ROMType type, const std::vector<uint8_t> &data) = 0;
};

}

#endif /* AmstradCPC_hpp */
