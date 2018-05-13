//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MSX_hpp
#define MSX_hpp

#include "../../Configurable/Configurable.hpp"

namespace MSX {

class Machine {
	public:
		virtual ~Machine();
		static Machine *MSX();
};

std::vector<std::unique_ptr<Configurable::Option>> get_options();

}

#endif /* MSX_hpp */
