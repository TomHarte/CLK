//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Tape_hpp
#define Tape_hpp

#include <stdio.h>

namespace Storage {

class Tape {
	public:

		struct Time {
			unsigned int length, clock_rate;
		};

		struct Cycle {
			enum {
				High, Low, Zero
			} type;
			Time length;
		};

		virtual Cycle get_next_cycle() = 0;
		virtual void reset() = 0;

		virtual void seek(Time seek_time);
};

}


#endif /* Tape_hpp */
