//
//  DiskROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "MemorySlotHandler.hpp"

#include "../../Activity/Source.hpp"
#include "../../Components/1770/1770.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace MSX {

class DiskROM: public MemorySlotHandler, public WD::WD1770 {
	public:
		DiskROM(MSX::MemorySlot &slot);

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) final;
		uint8_t read(uint16_t address) final;
		void run_for(HalfCycles half_cycles) final;

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive);
		void set_activity_observer(Activity::Observer *observer);

	private:
		const std::vector<uint8_t> &rom_;

		long int controller_cycles_ = 0;

		void set_head_load_request(bool head_load) final;
};

}
