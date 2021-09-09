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

		template <int id, int shift> void set_pointer(uint16_t value) {
			addresses_[id] = (addresses_[id] & (0xffff'0000 >> shift)) | uint32_t(value << shift);
		}

		void set_size(uint16_t value);
		void set_minterms(uint16_t value);
		void set_vertical_size(uint16_t value);
		void set_horizontal_size(uint16_t value);
		void set_modulo(int channel, uint16_t value);
		void set_data(int channel, uint16_t value);

		uint16_t get_status();

		bool advance();

	private:
		uint16_t *const ram_;
		const uint32_t ram_mask_;

		uint32_t addresses_[4];
		uint8_t minterms_;
		int width_ = 0, height_ = 0;
};

}


#endif /* Blitter_hpp */
