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
#include "DMADevice.hpp"

namespace Amiga {

class Blitter: public DMADevice<4> {
	public:
		using DMADevice::DMADevice;

		// Various setters; it's assumed that address decoding is handled externally.
		//
		// In all cases where a channel is identified numerically, it's taken that
		// 0 = A, 1 = B, 2 = C, 3 = D.
		void set_control(int index, uint16_t value);
		void set_first_word_mask(uint16_t value);
		void set_last_word_mask(uint16_t value);

		void set_size(uint16_t value);
		void set_minterms(uint16_t value);
		void set_vertical_size(uint16_t value);
		void set_horizontal_size(uint16_t value);
		void set_modulo(int channel, uint16_t value);
		void set_data(int channel, uint16_t value);

		uint16_t get_status();

		bool advance();

	private:
		int width_ = 0, height_ = 0;
		int shifts_[2]{};
		uint16_t a_mask_[2] = {0xffff, 0xffff};

		bool line_mode_ = false;
		bool one_dot_ = false;
		int line_direction_ = 0;
		int line_sign_ = 1;

		uint32_t direction_ = 1;
		bool inclusive_fill_ = false;
		bool exclusive_fill_ = false;
		bool fill_carry_ = false;

		bool channel_enables_[4]{};

		uint8_t minterms_ = 0;
		uint32_t a32_ = 0, b32_ = 0;
		uint16_t a_data_ = 0, b_data_ = 0, c_data_ = 0;
		uint16_t a_ = 0, b_ = 0;
		uint32_t modulos_[4]{};

};

}


#endif /* Blitter_hpp */
