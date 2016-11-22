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

		enum PagingFlags {
			BASICDisable	=	(1 << 0),
			MicrodscDisable	=	(1 << 1)
		};

		class Delegate {
			public:
				virtual void microdisc_did_change_paging_flags(Microdisc *microdisc) = 0;
		};
		inline void set_microdisc_delegate(Delegate *delegate)	{	delegate_ = delegate;	}
		inline int get_paging_flags()							{	return paging_flags_;	}

	private:
		std::shared_ptr<Storage::Disk::Drive> drives_[4];
		int selected_drive_;
		bool irq_enable_;
		int paging_flags_;
		Delegate *delegate_;
};

}

#endif /* Microdisc_hpp */
