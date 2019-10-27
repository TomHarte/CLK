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
#include "../../Components/1770/1770.hpp"

namespace Atari {
namespace ST {

class DMAController {
	public:
		DMAController();

		uint16_t read(int address);
		void write(int address, uint16_t value);
		void run_for(HalfCycles duration);

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

		uint16_t control_ = 0;
		uint32_t address_ = 0;
		uint16_t status_ = 0;
		uint16_t sector_count_ = 0;
};

}
}

#endif /* DMAController_hpp */
