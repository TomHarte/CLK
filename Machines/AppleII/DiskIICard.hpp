//
//  DiskII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef DiskIICard_hpp
#define DiskIICard_hpp

#include "Card.hpp"
#include "../ROMMachine.hpp"

#include "../../Components/DiskII/DiskII.hpp"
#include "../../Storage/Disk/Disk.hpp"
#include "../../ClockReceiver/Sleeper.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace AppleII {

class DiskIICard: public Card, public Sleeper::SleepObserver {
	public:
		DiskIICard(const ROMMachine::ROMFetcher &rom_fetcher, bool is_16_sector);

		void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) override;
		void run_for(Cycles cycles, int stretches) override;

		void set_activity_observer(Activity::Observer *observer) override;

		void set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive);

	private:
		void set_component_is_sleeping(Sleeper *component, bool is_sleeping) override;
		std::vector<uint8_t> boot_;
		Apple::DiskII diskii_;
		bool diskii_is_sleeping_ = false;
};

}

#endif /* DiskIICard_hpp */
