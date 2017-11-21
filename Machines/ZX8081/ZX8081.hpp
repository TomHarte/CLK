//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX8081_hpp
#define ZX8081_hpp

#include "../../Configurable/Configurable.hpp"
#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include <cstdint>
#include <vector>

namespace ZX8081 {

enum ROMType: uint8_t {
	ZX80 = 0, ZX81
};

/// @returns The options available for a ZX80 or ZX81.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine,
	public Configurable::Device {
	public:
		static Machine *ZX8081(const StaticAnalyser::Target &target_hint);
		virtual ~Machine();

		virtual void set_rom(ROMType type, const std::vector<uint8_t> &data) = 0;

		virtual void set_use_fast_tape_hack(bool activate) = 0;
		virtual void set_tape_is_playing(bool is_playing) = 0;
		virtual void set_use_automatic_tape_motor_control(bool enabled) = 0;
};

}

#endif /* ZX8081_hpp */
