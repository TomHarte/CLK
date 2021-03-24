//
//  Jasmin.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Jasmin_hpp
#define Jasmin_hpp

#include "../../Components/1770/1770.hpp"
#include "../../Activity/Observer.hpp"
#include "DiskController.hpp"

namespace Oric {

class Jasmin: public DiskController {
	public:
		Jasmin();

		void write(int address, uint8_t value);

		void set_activity_observer(Activity::Observer *observer);

	private:
		void set_motor_on(bool on) final;
		bool motor_on_ = false;
		uint8_t selected_drives_ = 0;

		Activity::Observer *observer_ = nullptr;

		bool enable_overlay_ram_ = false;
		bool disable_basic_rom_ = false;
		void select_paged_item() {
			PagedItem item = PagedItem::RAM;
			if(!enable_overlay_ram_) {
				item = disable_basic_rom_ ? PagedItem::DiskROM : PagedItem::BASIC;
			}
			set_paged_item(item);
		}
};

};

#endif /* Jasmin_hpp */
