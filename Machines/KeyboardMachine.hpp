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
		/*!
			Indicates that the key @c key has been either pressed or released, according to
			the state of @c isPressed.
		*/
		virtual void set_key_state(uint16_t key, bool isPressed) = 0;

		/*!
			Instructs that all keys should now be treated as released.
		*/
		virtual void clear_all_keys() = 0;
};

}

#endif /* KeyboardMachine_h */
