//
//  IWM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef IWM_hpp
#define IWM_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>

namespace Apple {

class IWM {
	public:
		IWM(int clock_rate);

		/// Sets the current external value of the data bus.
		void write(int address, uint8_t value);

		/*!
			Submits an access to address @c address.

			@returns The 8-bit value loaded to the data bus by the IWM.
		*/
		uint8_t read(int address);

		/// Advances the controller by @c cycles.
		void run_for(const Cycles cycles);

	private:
		uint8_t mode_ = 0;
		bool read_write_ready_ = true;
		bool write_overran_ = false;

		int q_switches_ = 0;

		void access(int address);
};


}

#endif /* IWM_hpp */
