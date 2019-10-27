//
//  DMAController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DMAController_hpp
#define DMAController_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Components/1770/1770.hpp"

namespace Atari {
namespace ST {

class DMAController {
	public:
		DMAController();

		uint16_t read(int address);
		void write(int address, uint16_t value);
		void run_for(HalfCycles duration);

	private:
		HalfCycles running_time_;
		WD::WD1770 fdc_;

		uint16_t control_ = 0;
		uint32_t address_ = 0;
		uint16_t status_ = 0;
		uint16_t sector_count_ = 0;
};

}
}

#endif /* DMAController_hpp */
