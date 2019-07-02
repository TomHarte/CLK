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

class IWM:
	public Storage::Disk::Drive::EventDelegate {
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

		/// Connects a drive to the IWM.
		void set_drive(int slot, Storage::Disk::Drive *drive);

	private:
		// Storage::Disk::Drive::EventDelegate.
		void process_event(const Storage::Disk::Drive::Event &event) override;

		const int clock_rate_;

		uint8_t data_register_ = 0;
		uint8_t mode_ = 0;
		bool read_write_ready_ = true;
		bool write_overran_ = false;

		int state_ = 0;

		int active_drive_ = 0;
		Storage::Disk::Drive *drives_[2] = {nullptr, nullptr};
		bool drive_motor_on_ = false;

		Cycles cycles_until_motor_off_;

		void access(int address);

		uint8_t shift_register_ = 0;
		void propose_shift(uint8_t bit);
		Cycles cycles_since_shift_;
		Cycles bit_length_;

		int step_direction_ = 0;	// TODO: this should live on the drive.
};


}

#endif /* IWM_hpp */
