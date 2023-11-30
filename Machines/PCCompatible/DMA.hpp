//
//  DMA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef DMA_hpp
#define DMA_hpp

#include "../../Numeric/RegisterSizes.hpp"

namespace PCCompatible {

class i8237 {
	public:
		void flip_flop_reset() {
			next_access_low = true;
		}

		void mask_reset() {
			// TODO: set all mask bits off.
		}

		void master_reset() {
			flip_flop_reset();
			// TODO: clear status, set all mask bits on.
		}

		template <int address>
		void write(uint8_t value) {
			constexpr int channel = (address >> 1) & 3;
			constexpr bool is_count = address & 1;

			next_access_low ^= true;
			if(next_access_low) {
				if constexpr (is_count) {
					channels_[channel].count.halves.high = value;
				} else {
					channels_[channel].address.halves.high = value;
				}
			} else {
				if constexpr (is_count) {
					channels_[channel].count.halves.low = value;
				} else {
					channels_[channel].address.halves.low = value;
				}
			}
		}

		template <int address>
		uint8_t read() {
			constexpr int channel = (address >> 1) & 3;
			constexpr bool is_count = address & 1;

			next_access_low ^= true;
			if(next_access_low) {
				if constexpr (is_count) {
					return channels_[channel].count.halves.high;
				} else {
					return channels_[channel].address.halves.high;
				}
			} else {
				if constexpr (is_count) {
					return channels_[channel].count.halves.low;
				} else {
					return channels_[channel].address.halves.low;
				}
			}
		}

	private:
		bool next_access_low = true;

		struct Channel {
			CPU::RegisterPair16 address, count;
		} channels_[4];
};

class DMAPages {
	public:
		template <int index>
		void set_page(uint8_t value) {
			pages_[page_for_index(index)] = value;
		}

		template <int index>
		uint8_t page() {
			return pages_[page_for_index(index)];
		}

		uint8_t channel_page(int channel) {
			return pages_[channel];
		}

	private:
		uint8_t pages_[8];

		constexpr int page_for_index(int index) {
			switch(index) {
				case 7:		return 0;
				case 3:		return 1;
				case 1:		return 2;
				case 2:		return 3;

				default:
				case 0:		return 4;
				case 4:		return 5;
				case 5:		return 6;
				case 6:		return 7;
			}
		}
};

}

#endif /* DMA_hpp */
