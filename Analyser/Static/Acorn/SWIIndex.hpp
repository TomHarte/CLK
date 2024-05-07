//
//  SWIIndex.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#ifndef SWIIndex_hpp
#define SWIIndex_hpp

#include <cstdint>

namespace Analyser::Static::Acorn {

struct SWIDescription {

};

const SWIDescription &describe_swi(uint32_t comment);

}

#endif /* SWIIndex_hpp */
