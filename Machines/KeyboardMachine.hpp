//
//  KeyboardMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef KeyboardMachine_h
#define KeyboardMachine_h

namespace KeyboardMachine {

class Machine {
	public:
		virtual void set_key_state(uint16_t key, bool isPressed) = 0;
		virtual void clear_all_keys() = 0;
};

}

#endif /* KeyboardMachine_h */
