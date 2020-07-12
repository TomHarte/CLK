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
#include <vector>

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/ClockingHintSource.hpp"
#include "../../../Components/1770/1770.hpp"
#include "../../../Activity/Source.hpp"

namespace Atari {
namespace ST {

class DMAController: public WD::WD1770::Delegate, public ClockingHint::Source, public ClockingHint::Observer {
	public:
		DMAController();

		uint16_t read(int address);
		void write(int address, uint16_t value);
		void run_for(HalfCycles duration);

		bool get_interrupt_line();
		bool get_bus_request_line();

		/*!
			Indicates that the DMA controller has been granted bus access to the block of memory at @c ram, which
			is of size @c size.

			@returns The number of words read or written.
		*/
		int bus_grant(uint16_t *ram, size_t size);

		void set_floppy_drive_selection(bool drive1, bool drive2, bool side2);
		void set_floppy_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive);

		struct Delegate {
			virtual void dma_controller_did_change_output(DMAController *) = 0;
		};
		void set_delegate(Delegate *delegate);

		void set_activity_observer(Activity::Observer *observer);

		// ClockingHint::Source.
		ClockingHint::Preference preferred_clocking() const final;

	private:
		HalfCycles running_time_;
		struct WD1772: public WD::WD1770 {
			WD1772(): WD::WD1770(WD::WD1770::P1772) {
				emplace_drives(2, 8000000, 300, 2);
				set_is_double_density(true);	// TODO: is this selectable on the ST?
			}

			void set_motor_on(bool motor_on) final {
				for_all_drives([motor_on] (Storage::Disk::Drive &drive, size_t) {
					drive.set_motor_on(motor_on);
				});
			}

			void set_floppy_drive_selection(bool drive1, bool drive2, bool side2) {
				set_drive(
					(drive1 ? 1 : 0) |
					(drive2 ? 2 : 0)
				);

				for_all_drives([side2] (Storage::Disk::Drive &drive, size_t) {
					drive.set_head(side2);
				});
			}

			void set_activity_observer(Activity::Observer *observer) {
				get_drive(0).set_activity_observer(observer, "Internal", true);
				get_drive(1).set_activity_observer(observer, "External", true);
			}

			void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
				get_drive(drive).set_disk(disk);
			}

		} fdc_;

		void wd1770_did_change_output(WD::WD1770 *) final;

		uint16_t control_ = 0;

		Delegate *delegate_ = nullptr;
		bool interrupt_line_ = false;
		bool bus_request_line_ = false;

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final;

		// MARK: - DMA State.
		struct Buffer {
			uint8_t contents[16];
			bool is_full = false;
		} buffer_[2];
		int active_buffer_ = 0;
		int bytes_received_ = 0;
		bool error_ = false;
		int address_ = 0;
		int byte_count_ = 0;
};

}
}

#endif /* DMAController_hpp */
