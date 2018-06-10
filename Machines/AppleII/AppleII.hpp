//
//  AppleII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef AppleII_hpp
#define AppleII_hpp

#include "../../Configurable/Configurable.hpp"

#include <memory>
#include <vector>

namespace AppleII {

/// @returns The options available for an Apple II.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an AppleII.
		static Machine *AppleII();
};

};

#endif /* AppleII_hpp */
