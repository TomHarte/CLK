//
//  Blitter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Blitter_hpp
#define Blitter_hpp

#include <cstddef>
#include <cstdint>

#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Amiga {

class Blitter {
	public:
		Blitter(uint16_t *ram, size_t size);

		// Various setters; it's assumed that address decoding is handled externally.
		//
		// In all cases where a channel is identified numerically, it's taken that
		// 0 = A, 1 = B, 2 = C, 3 = D.
		void set_control(int index, uint16_t value);
		void set_first_word_mask(uint16_t value);
		void set_last_word_mask(uint16_t value);
		void set_pointer(int channel, int shift, uint16_t value);
		void set_size(uint16_t value);
		void set_minterms(uint16_t value);
		void set_vertical_size(uint16_t value);
		void set_horizontal_size(uint16_t value);
		void set_modulo(int channel, uint16_t value);
		void set_data(int channel, uint16_t value);

		uint16_t get_status();

		/// @returns The number of 'cycles' required to complete the current
		/// operation, if any, or Cycles(0) if no operation is pending.
		Cycles get_remaining_cycles();

		/// Advances the stated number of cycles.
		void run_for(Cycles);

	private:
		uint16_t *const ram_;
		const size_t ram_size_;
};

}


#endif /* Blitter_hpp */
