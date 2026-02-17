//
//  ROMLibrary.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/Utility/ROMCatalogue.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ROM {

/*!
	Provides the contents of a named ROM if and only if it is one that can legally be built into
	this emulator's executable. Some vendors have allowed redistribution of their material,
	most haven't.
*/
std::optional<std::vector<uint8_t>> included_rom_image(ROM::Name);

}
