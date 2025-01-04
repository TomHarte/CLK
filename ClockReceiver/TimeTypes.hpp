//
//  TimeTypes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include <chrono>

namespace Time {

using Seconds = double;
using Nanos = int64_t;

inline Nanos nanos_now() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()
	).count();
}

inline Seconds seconds(Nanos nanos) {
	return double(nanos) / 1e9;
}

}
