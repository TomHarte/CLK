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

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../../Components/1770/1770.hpp"

namespace Atari {
namespace ST {

class DMAController: public WD::WD1770::Delegate, public ClockingHint::Source, public ClockingHint::Observer {
	public:
		DMAController();

		uint16_t read(int address);
		void write(int address, uint16_t value);
		void run_for(HalfCycles duration);

		bool get_interrupt_line();

		struct InterruptDelegate {
			virtual void dma_controller_did_change_interrupt_status(DMAController *) = 0;
		};
		void set_interrupt_delegate(InterruptDelegate *delegate);

		// ClockingHint::Source.
		ClockingHint::Preference preferred_clocking() final;

	private:
		HalfCycles running_time_;
		struct WD1772: public WD::WD1770 {
			WD1772(): WD::WD1770(WD::WD1770::P1772) {
				drives_.emplace_back(new Storage::Disk::Drive(8000000, 300, 2));
				drives_.emplace_back(new Storage::Disk::Drive(8000000, 300, 2));
				set_drive(drives_[0]);
			}

			void set_motor_on(bool motor_on) final {
				drives_[0]->set_motor_on(motor_on);
				drives_[1]->set_motor_on(motor_on);
			}

			std::vector<std::shared_ptr<Storage::Disk::Drive>> drives_;
		} fdc_;

		void wd1770_did_change_output(WD::WD1770 *) final;

		uint16_t control_ = 0;
		uint32_t address_ = 0;
		uint16_t status_ = 0;
		uint16_t sector_count_ = 0;

		InterruptDelegate *interrupt_delegate_ = nullptr;
		bool interrupt_line_ = false;

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final;
};

}
}

#endif /* DMAController_hpp */
