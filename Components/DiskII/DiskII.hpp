//
//  DiskII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef DiskII_hpp
#define DiskII_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"

#include "../../Storage/Disk/Disk.hpp"
#include "../../Storage/Disk/Drive.hpp"

#include "../../Activity/Observer.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Apple {

/*!
	Provides an emulation of the Apple Disk II.
*/
class DiskII final:
	public Storage::Disk::Drive::EventDelegate,
	public ClockingHint::Source,
	public ClockingHint::Observer {
	public:
		DiskII(int clock_rate);

		/// Sets the current external value of the data bus.
		void set_data_input(uint8_t input);

		/*!
			Submits an access to address @c address.

			@returns The 8-bit value loaded to the data bus by the DiskII if any;
				@c DidNotLoad otherwise.
		*/
		int read_address(int address);

		/*!
			The value returned by @c read_address if accessing that address
			didn't cause the disk II to place anything onto the bus.
		*/
		const int DidNotLoad = -1;

		/// Advances the controller by @c cycles.
		void run_for(const Cycles cycles);

		/*!
			Supplies the image of the state machine (i.e. P6) ROM,
			which dictates how the Disk II will respond to input.

			To reduce processing costs, some assumptions are made by
			the implementation as to the content of this ROM.
			Including:

			*	If Q6 is set and Q7 is reset, the controller is testing
				for write protect. If and when the shift register has
				become full with the state of the write protect value,
				no further processing is required.

			*	If both Q6 and Q7 are reset, the drive motor is disabled,
				and the shift register is all zeroes, no further processing
				is required.
		*/
		void set_state_machine(const std::vector<uint8_t> &);

		/// Inserts @c disk into the drive @c drive.
		void set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive);

		// As per Sleeper.
		ClockingHint::Preference preferred_clocking() final;

		// The Disk II functions as a potential target for @c Activity::Sources.
		void set_activity_observer(Activity::Observer *observer);

		// Returns the Storage::Disk::Drive in use for drive @c index.
		// *NOT FOR HARDWARE EMULATION USAGE*.
		Storage::Disk::Drive &get_drive(int index);

	private:
		enum class Control {
			P0, P1, P2, P3,
			Motor,
		};
		enum class Mode {
			Read, Write
		};
		void set_control(Control control, bool on);
		void set_mode(Mode mode);
		void select_drive(int drive);

		uint8_t trigger_address(int address, uint8_t value);
		void process_event(const Storage::Disk::Drive::Event &event) override;
		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference preference) override;

		const int clock_rate_ = 0;

		uint8_t state_ = 0;
		uint8_t inputs_ = 0;
		uint8_t shift_register_ = 0;

		int stepper_mask_ = 0;
		int stepper_position_ = 0;
		int motor_off_time_ = -1;

		bool is_write_protected();
		std::array<uint8_t, 256> state_machine_;
		Storage::Disk::Drive drives_[2];
		bool drive_is_sleeping_[2];
		int active_drive_ = 0;
		bool motor_is_enabled_ = false;

		void decide_clocking_preference();
		ClockingHint::Preference clocking_preference_ = ClockingHint::Preference::RealTime;

		uint8_t data_input_ = 0;
		int flux_duration_ = 0;
};

}

#endif /* DiskII_hpp */
