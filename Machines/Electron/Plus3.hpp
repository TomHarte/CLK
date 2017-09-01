//
//  Plus3.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Plus3_hpp
#define Plus3_hpp

#include "../../Components/1770/1770.hpp"

namespace Electron {

class Plus3 : public WD::WD1770 {
	public:
		Plus3();

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);
		void set_control_register(uint8_t control);

	private:
		void set_control_register(uint8_t control, uint8_t changes);
		std::shared_ptr<Storage::Disk::Drive> drives_[2];
		int selected_drive_ = 0;
		uint8_t last_control_ = 0;
};

}

#endif /* Plus3_hpp */

