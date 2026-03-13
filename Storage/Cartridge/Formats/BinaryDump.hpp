//
//  BinaryDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Cartridge/Cartridge.hpp"

#include <string_view>

namespace Storage::Cartridge {

class BinaryDump : public Cartridge {
public:
	BinaryDump(std::string_view file_name);

	enum {
		ErrorNotAccessible
	};
};

}
