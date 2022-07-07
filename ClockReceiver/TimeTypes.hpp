//
//  TimeTypes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef TimeTypes_h
#define TimeTypes_h

#include <chrono>

namespace Time {

typedef double Seconds;
typedef int64_t Nanos;

inline Nanos nanos_now() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

inline Seconds seconds(Nanos nanos) {
	return double(nanos) / 1e9;
}

}

#endif /* TimeTypes_h */

