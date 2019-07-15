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
#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../../Storage/Disk/Drive.hpp"

#include <cstdint>

namespace Apple {

/*!
	Defines the drive interface used by the IWM, derived from the external pinout as
	per e.g. https://old.pinouts.ru/HD/MacExtDrive_pinout.shtml

	These are subclassed of Storage::Disk::Drive, so accept any disk the emulator supports,
	and provide the usual read/write interface for on-disk data.
*/
struct IWMDrive: public Storage::Disk::Drive {
	IWMDrive(int input_clock_rate, int number_of_heads) : Storage::Disk::Drive(input_clock_rate, number_of_heads) {}

	enum Line: int {
		CA0		= 1 << 0,
		CA1		= 1 << 1,
		CA2		= 1 << 2,
		LSTRB	= 1 << 3,
		SEL		= 1 << 4,
	};

	virtual void set_enabled(bool) = 0;
	virtual void set_control_lines(int) = 0;
	virtual bool read() = 0;
};

class IWM:
	public Storage::Disk::Drive::EventDelegate,
	public ClockingHint::Observer {
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
		void set_drive(int slot, IWMDrive *drive);

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
		IWMDrive *drives_[2] = {nullptr, nullptr};
		bool drive_is_rotating_[2] = {false, false};

		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) override;

		Cycles cycles_until_disable_;
		uint8_t write_handshake_ = 0x80;

		void access(int address);

		uint8_t shift_register_ = 0;
		uint8_t next_output_ = 0;
		int output_bits_remaining_ = 0;

		void propose_shift(uint8_t bit);
		Cycles cycles_since_shift_;
		Cycles bit_length_;

		void push_drive_state();

		enum class ShiftMode {
			Reading,
			Writing,
			CheckingWriteProtect
		} shift_mode_;

		uint8_t sense();
		void select_shift_mode();
};


}

#endif /* IWM_hpp */
