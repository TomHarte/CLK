//
//  CommodoreROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreROM_hpp
#define CommodoreROM_hpp

#include <vector>
#include <cstdint>

namespace Storage::Cartridge::Encodings::CommodoreROM {

bool isROM(const std::vector<uint8_t> &);

}

#endif /* CommodoreROM_hpp */
