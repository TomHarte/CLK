//
//  PRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Cartridge/Cartridge.hpp"

#include <string_view>

namespace Storage::Cartridge {

class PRG : public Cartridge {
public:
	PRG(std::string_view file_name);

	enum {
		ErrorNotROM
	};
};

}
