//
//  RP5C01.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef RP5C01_hpp
#define RP5C01_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"

#include <array>
#include <cstdint>

namespace Ricoh {
namespace RP5C01 {

class RP5C01 {
	public:
		RP5C01(HalfCycles clock_rate);

		/// @returns the result of a read from @c address.
		uint8_t read(int address);

		/// Performs a write of @c value to @c address.
		void write(int address, uint8_t value);

		/// Advances time.
		void run_for(HalfCycles);

	private:
		std::array<uint8_t, 26> ram_;

		HalfCycles sub_seconds_;
		const HalfCycles clock_rate_;

		// Contains the seconds, minutes and hours fields.
		int seconds_ = 0;

		// Calendar entries.
		int day_of_the_week_ = 0;
		int day_ = 0;
		int month_ = 0;
		int year_ = 0;
		int leap_year_ = 0;

		// Other flags.
		bool timer_enabled_ = false;
		bool alarm_enabled_ = false;
		int mode_ = 0;
		bool one_hz_on_ = false;
		bool sixteen_hz_on_ = false;
};

}
}

#include <stdio.h>

#endif /* RP5C01_hpp */
