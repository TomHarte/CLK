//
//  BD500.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef BD500_hpp
#define BD500_hpp

#include "../../Components/1770/1770.hpp"
#include "../../Activity/Observer.hpp"
#include "DiskController.hpp"

#include <array>
#include <memory>

namespace Oric {

/*!
	Emulates a Byte Drive 500, at least to some extent. Very little is known about this interface,
	and I'm in possession of only a single disk image. So much of the below is community guesswork;
	see the thread at https://forum.defence-force.org/viewtopic.php?f=25&t=2055
*/
class BD500: public DiskController {
	public:
		BD500();

		void write(int address, uint8_t value);
		uint8_t read(int address);

		void run_for(const Cycles cycles);

		void set_activity_observer(Activity::Observer *observer);

	private:
		void set_head_load_request(bool head_load) final;
		bool is_loading_head_ = false;
		Activity::Observer *observer_ = nullptr;

		void access(int address);
		void set_head_loaded(bool loaded);

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

#endif /* BD500_hpp */
