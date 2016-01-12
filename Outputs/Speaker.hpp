//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Speaker_hpp
#define Speaker_hpp

#include <stdint.h>

namespace Speaker {

class Delegate {
	public:
		virtual void speaker_did_complete_samples(uint8_t *buffer);
};

template <class T> class Filter {
	public:
		void set_output_rate(int cycles_per_second);
		void set_delegate(Delegate *delegate);

		void set_input_rate(int cycles_per_second);
		void run_for_cycles(int input_cycles);
};

}

#endif /* Speaker_hpp */
