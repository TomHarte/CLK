//
//  StringSerialiser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>

namespace Utility {

class StringSerialiser {
public:
	StringSerialiser(const std::wstring &source, bool use_linefeed_only = false);

	wchar_t head();
	bool advance();

private:
	std::wstring input_string_;
	std::size_t input_string_pointer_ = 0;
};

}
