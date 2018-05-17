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
#include "../../ClockReceiver/Sleeper.hpp"

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
class DiskII:
	public Storage::Disk::Drive::EventDelegate,
	public Sleeper::SleepObserver,
	public Sleeper {
	public:
		DiskII();

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void run_for(const Cycles cycles);
		void set_state_machine(const std::vector<uint8_t> &);

		void set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive);
		bool is_sleeping() override;

		void set_activity_observer(Activity::Observer *observer);

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
		void set_data_register(uint8_t value);
		uint8_t get_shift_register();

		uint8_t trigger_address(int address, uint8_t value);
		void process_event(const Storage::Disk::Track::Event &event) override;
		void set_component_is_sleeping(Sleeper *component, bool is_sleeping) override;

		uint8_t state_ = 0;
		uint8_t inputs_ = 0;
		uint8_t shift_register_ = 0;

		int stepper_mask_ = 0;
		int stepper_position_ = 0;

		bool is_write_protected();
		std::array<uint8_t, 256> state_machine_;
		Storage::Disk::Drive drives_[2];
		bool drive_is_sleeping_[2];
		bool controller_can_sleep_ = false;
		int active_drive_ = 0;
		bool motor_is_enabled_ = false;

		void set_controller_can_sleep();
};

}

#endif /* DiskII_hpp */
