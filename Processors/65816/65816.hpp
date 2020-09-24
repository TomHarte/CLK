//
//  65816.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef WDC65816_hpp
#define WDC65816_hpp

#include <cstdint>
#include <vector>

namespace CPU {
namespace WDC65816 {

enum class Personality {
	WDC65816,
	WDC65802
};

#include "Implementation/65816Storage.hpp"

template <Personality personality> class Processor: public ProcessorStorage {

};

}
}

#endif /* WDC65816_hpp */
