//
//  Sprites.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Sprites_hpp
#define Sprites_hpp

#include <cstdint>

#include "DMADevice.hpp"

namespace Amiga {

class Sprite: public DMADevice<1> {
	public:
		using DMADevice::DMADevice;

		void set_start_position(uint16_t value);
		void set_stop_and_control(uint16_t value);
		void set_image_data(int slot, uint16_t value);

		void advance_line(int y, bool is_end_of_blank);
		bool advance_dma(int offset);

		uint16_t data[2]{};
		bool attached = false;
		bool visible = false;
		uint16_t h_start = 0;

	private:
		uint16_t v_start_ = 0, v_stop_ = 0;

		enum class DMAState {
			FetchControl,
			FetchImage
		} dma_state_ = DMAState::FetchControl;
};

class TwoSpriteShifter {
	public:
		/// Installs new pixel data for @c sprite (either 0 or 1),
		/// with @c delay being either 0 or 1 to indicate whether
		/// output should begin now or in one pixel's time.
		template <int sprite> void load(
			uint16_t lsb,
			uint16_t msb,
			int delay);

		/// Shifts two pixels.
		void shift() {
			data_ <<= 8;
			data_ |= overflow_;
			overflow_ = 0;
		}

		/// @returns The next two pixels to output, formulated as
		/// abcd efgh where ab and ef are two pixels of the first sprite
		/// and cd and gh are two pixels of the second. In each case the
		/// more significant two are output first.
		uint8_t get() {
			return uint8_t(data_ >> 56);
		}

	private:
		uint64_t data_;
		uint8_t overflow_;
};


}

#endif /* Sprites_hpp */
