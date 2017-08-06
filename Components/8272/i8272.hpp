//
//  i8272.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef i8272_hpp
#define i8272_hpp

#include "../../Storage/Disk/MFMDiskController.hpp"

#include <cstdint>
#include <vector>

namespace Intel {

class i8272: public Storage::Disk::MFMController {
	public:
		i8272(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

		void run_for(Cycles);

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

	private:
		void posit_event(Event type);
		uint8_t status_;
		std::vector<uint8_t> command_;
};

}


#endif /* i8272_hpp */
