//
//  Microdisc.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Microdisc_hpp
#define Microdisc_hpp

#include "../../Components/1770/1770.hpp"

namespace Oric {

class Microdisc: public WD::WD1770 {
	public:
		Microdisc();

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);
		void set_control_register(uint8_t control);
		uint8_t get_interrupt_request_register();
		uint8_t get_data_request_register();

		bool get_interrupt_request_line();

	private:
		std::shared_ptr<Storage::Disk::Drive> drives_[4];
		int selected_drive_;
		bool irq_enable_;
};

}

#endif /* Microdisc_hpp */
