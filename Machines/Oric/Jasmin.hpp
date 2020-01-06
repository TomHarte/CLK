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

#include <array>
#include <memory>

namespace Oric {

class Jasmin: public WD::WD1770 {
	public:
		Jasmin();

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);
		void write(int address, uint8_t value);

		enum PagingFlags {
			/// Indicates that overlay RAM is enabled, implying no ROM is visible.
			OverlayRAMEnable		=	(1 << 0),

			/// Indicates that the BASIC ROM is disabled, implying that the JASMIN ROM
			/// fills its space.
			BASICDisable			=	(1 << 1)
		};
		struct Delegate: public WD1770::Delegate {
			virtual void jasmin_did_change_paging_flags(Jasmin *jasmin) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;	WD1770::set_delegate(delegate);	}
		inline int get_paging_flags()					{	return paging_flags_;									}

	private:
		std::array<std::shared_ptr<Storage::Disk::Drive>, 4> drives_;
		size_t selected_drive_;
		int paging_flags_ = 0;
		Delegate *delegate_ = nullptr;

		void posit_paging_flags(int new_flags) {
			if(new_flags != paging_flags_) {
				paging_flags_ = new_flags;
				if(delegate_) delegate_->jasmin_did_change_paging_flags(this);
			}
		}

		void set_motor_on(bool on) final;
		bool motor_on_ = false;
};

};

#endif /* Jasmin_hpp */
