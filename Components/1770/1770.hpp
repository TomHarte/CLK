//
//  1770.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _770_hpp
#define _770_hpp

#include "../../Storage/Disk/DiskDrive.hpp"

namespace WD {

class WD1770 {
	public:

		void set_drive(std::shared_ptr<Storage::Disk::Drive> drive);
		void set_is_double_density(bool is_double_density);
		void set_register(int address, uint8_t value);
		void get_register(int address);
};

}

#endif /* _770_hpp */
