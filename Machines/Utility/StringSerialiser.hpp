//
//  StringSerialiser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef StringSerialiser_hpp
#define StringSerialiser_hpp

#include <string>

namespace Utility {

class StringSerialiser {
	public:
		StringSerialiser(const std::string &source, bool use_linefeed_only = false);

		uint8_t head();
		bool advance();

	private:
		std::string input_string_;
		std::size_t input_string_pointer_ = 0;
};

}

#endif /* StringSerialiser_hpp */
