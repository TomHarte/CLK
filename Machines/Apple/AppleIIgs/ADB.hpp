//
//  ADB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_ADB_hpp
#define Apple_IIgs_ADB_hpp

#include <cstdint>

namespace Apple {
namespace IIgs {
namespace ADB {

class GLU {
	public:
		uint8_t get_keyboard_data();
		uint8_t get_mouse_data();
		uint8_t get_modifier_status();
		uint8_t get_any_key_down();

		uint8_t get_data();
		uint8_t get_status();

		void set_command(uint8_t);
		void set_status(uint8_t);
		void clear_key_strobe();
};

}
}
}

#endif /* Apple_IIgs_ADB_hpp */
