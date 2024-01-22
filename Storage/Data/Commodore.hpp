//
//  Commodore.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>

namespace Storage::Data::Commodore {

std::wstring petscii_from_bytes(const uint8_t *string, int length, bool shifted);

}
