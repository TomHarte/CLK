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
#include "../../Storage/Disk/Drive.hpp"

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

		/*!
			Sets the current input of the IWM's SEL line.
		*/
		void set_select(bool enabled);

		/// Advances the controller by @c cycles.
		void run_for(const Cycles cycles);

	private:
		const int clock_rate_;

		uint8_t mode_ = 0;
		bool read_write_ready_ = true;
		bool write_overran_ = false;

		int state_ = 0;

		int active_drive_ = 0;
		Storage::Disk::Drive drives_[2];

		Cycles cycles_until_motor_off_;

		void access(int address);

		Cycles bit_length_;
};


}

#endif /* IWM_hpp */
