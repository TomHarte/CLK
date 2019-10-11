//
//  6850.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Motorola_ACIA_6850_hpp
#define Motorola_ACIA_6850_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Motorola {
namespace ACIA {

class ACIA {
	public:
		uint8_t read(int address);
		void write(int address, uint8_t value);

		void run_for(HalfCycles);
};

}
}

#endif /* Motorola_ACIA_6850_hpp */
