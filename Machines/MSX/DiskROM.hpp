//
//  DiskROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef DiskROM_hpp
#define DiskROM_hpp

#include "ROMSlotHandler.hpp"

#include "../../Activity/Source.hpp"
#include "../../Components/1770/1770.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace MSX {

class DiskROM: public ROMSlotHandler, public WD::WD1770 {
	public:
		DiskROM(const std::vector<uint8_t> &rom);

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) final;
		uint8_t read(uint16_t address) final;
		void run_for(HalfCycles half_cycles) final;

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive);
		void set_activity_observer(Activity::Observer *observer);

	private:
		const std::vector<uint8_t> &rom_;

		long int controller_cycles_ = 0;
		size_t selected_drive_ = 0;
		int selected_head_ = 0;
		std::array<std::shared_ptr<Storage::Disk::Drive>, 2> drives_;

		void set_head_load_request(bool head_load) final;
};

}

#endif /* DiskROM_hpp */
