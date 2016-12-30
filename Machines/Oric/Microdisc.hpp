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

		void run_for_cycles(unsigned int number_of_cycles);

		enum PagingFlags {
			BASICDisable	=	(1 << 0),
			MicrodscDisable	=	(1 << 1)
		};

		class Delegate: public WD1770::Delegate {
			public:
				virtual void microdisc_did_change_paging_flags(Microdisc *microdisc) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;	WD1770::set_delegate(delegate);	}
		inline int get_paging_flags()					{	return paging_flags_;									}

	private:
		void set_control_register(uint8_t control, uint8_t changes);
		void set_head_load_request(bool head_load);
		bool get_drive_is_ready();
		std::shared_ptr<Storage::Disk::Drive> drives_[4];
		int selected_drive_;
		bool irq_enable_;
		int paging_flags_;
		int head_load_request_counter_;
		Delegate *delegate_;
		uint8_t last_control_;
};

}

#endif /* Microdisc_hpp */
