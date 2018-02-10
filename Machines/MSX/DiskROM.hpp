//
//  DiskROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef DiskROM_hpp
#define DiskROM_hpp

#include "ROMSlotHandler.hpp"

#include "../../Components/1770/1770.hpp"

#include <cstdint>
#include <vector>

namespace MSX {

class DiskROM: public ROMSlotHandler, public WD::WD1770 {
	public:
		DiskROM(const std::vector<uint8_t> &rom);

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) override;
		uint8_t read(uint16_t address) override;
		void run_for(HalfCycles half_cycles) override;

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);

	private:
		const std::vector<uint8_t> &rom_;

		long int controller_cycles_ = 0;
		int selected_drive_ = 0;
		int selected_head_ = 0;
		std::shared_ptr<Storage::Disk::Drive> drives_[4];

		void set_head_load_request(bool head_load) override;
};

}

#endif /* DiskROM_hpp */
