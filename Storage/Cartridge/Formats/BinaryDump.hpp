//
//  BinaryDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Cartridge/Cartridge.hpp"

#include <string>

namespace Storage::Cartridge {

class BinaryDump : public Cartridge {
public:
	BinaryDump(const std::string &file_name);

	enum {
		ErrorNotAccessible
	};
};

}
