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
		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);
		void set_control_register(uint8_t control);

	private:
		std::shared_ptr<Storage::Disk::Drive> _drives[2];
};

}

#endif /* Plus3_hpp */

