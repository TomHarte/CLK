//
//  Commodore.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Data_Commodore_hpp
#define Storage_Data_Commodore_hpp

#include <string>

namespace Storage {
namespace Data {
namespace Commodore {

std::wstring petscii_from_bytes(const uint8_t *string, int length, bool shifted);

}
}
}

#endif /* Commodore_hpp */
