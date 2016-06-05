//
//  6560.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _560_hpp
#define _560_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace MOS {

class MOS6560 {
	public:
		MOS6560();
		Outputs::CRT::CRT *get_crt() { return _crt.get(); }

		uint16_t get_address();
		void set_graphics_value(uint8_t value);

		void set_register(int address, uint8_t value);

	private:
		std::unique_ptr<Outputs::CRT::CRT> _crt;
};

}

#endif /* _560_hpp */
