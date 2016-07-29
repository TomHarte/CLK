//
//  CommodoreGCR.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreGCR_hpp
#define CommodoreGCR_hpp

#include "../../Storage.hpp"

namespace Storage {
namespace Encodings {
namespace CommodoreGCR {
	Time length_of_a_bit_in_time_zone(unsigned int time_zone);
}
}
}

#endif /* CommodoreGCR_hpp */
