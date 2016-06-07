//
//  6522.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _522_hpp
#define _522_hpp

#include <cstdint>

namespace MOS {

class MOS6522 {
	public:
		MOS6522();

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);
};

}

#endif /* _522_hpp */
