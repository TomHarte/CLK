//
//  2MG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "2MG.hpp"

using namespace Storage::Disk;

DiskImageHolderBase *Disk2MG::open(const std::string &file_name) {
	(void)file_name;
	throw Error::InvalidFormat;
	return nullptr;
}
