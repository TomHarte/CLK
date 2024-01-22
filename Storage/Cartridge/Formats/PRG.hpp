//
//  PRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Cartridge.hpp"

#include <string>

namespace Storage::Cartridge {

class PRG : public Cartridge {
	public:
		PRG(const std::string &file_name);

		enum {
			ErrorNotROM
		};
};

}
