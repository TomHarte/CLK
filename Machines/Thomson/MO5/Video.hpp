//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "ClockReceiver/ClockReceiver.hpp"

namespace Thomson::MO5 {

struct Video {
public:
	void run_for(Cycles);
};

}

#endif /* Video_hpp */
