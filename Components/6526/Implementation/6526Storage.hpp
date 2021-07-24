//
//  6526Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Storage_h
#define _526Storage_h

namespace MOS {
namespace MOS6526 {

struct MOS6526Storage {
	HalfCycles half_divider_;

	uint8_t output_[2] = {0, 0};
	uint8_t data_direction_[2] = {0, 0};
	uint8_t interrupt_control_ = 0;
	uint8_t control_[2] = {0, 0};

	uint32_t tod_increment_mask_ = uint32_t(~0);
	uint32_t tod_latch_ = 0;
	uint32_t tod_ = 0;

	bool write_tod_alarm_ = false;
	uint32_t tod_alarm_ = 0;

	struct Counter {
		uint16_t reload = 0;
		uint16_t value = 0;
	} counters_[2];
};

}
}

#endif /* _526Storage_h */
