//
//  Keyboard.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

namespace Apple {
namespace Macintosh {

class Keyboard {
	public:
		void set_input(bool data) {
		}

		bool get_clock() {
			return false;
		}

		bool get_data() {
			return false;
		}

		/*!
			The keyboard expects ~10 µs-frequency ticks, i.e. a clock rate of just around 100 kHz.
		*/
		void run_for(HalfCycles cycle) {
			// TODO: something.
		}
};

/*
	"When sending data to the computer, the keyboard transmits eight cycles of 330 µS each (160 µS low, 170 µS high)
	on the normally high Keyboard Clock line. It places a data bit on the data line 40 µS before the falling edge of each
	clock cycle and maintains it for 330 µS. The VIA in the compu(er latches the data bit into its Shift register on the
	rising edge of the Keyboard Clock signal."

	"When the computer is sending data to the keyboard, the keyboard transmits eight cycles of 400 µS each (180 µS low,
	220 µS high) on the Keyboard Clock line. On the falling edge of each keyboard clock cycle, the Macintosh Plus places
	a data bit on the data line and holds it there for 400 µS. The keyboard reads the data bit 80 µS after the rising edge
	of the Keyboard Clock signal."
*/

}
}

#endif /* Keyboard_h */
