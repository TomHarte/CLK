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

class BlitterSequencer {
	public:
		enum class Channel {
			Write, C, B, A, None
		};

		void set_control(int control) {
			control_ = control;
		}

		void complete() {
			complete_ = true;
		}

		Channel next() {
			if(complete_ && !index_) {
				// TODO: this isn't quite right; some patterns leave a gap before
				// the final write, some don't. Figure this out.
				return Channel::Write;
			}

			Channel next = Channel::None;

			switch(control_) {
				default: break;

				case 1: next = next_channel(pattern1); break;
				case 2: next = next_channel(pattern2); break;
				case 3: next = next_channel(pattern3); break;
				case 4: next = next_channel(pattern4); break;
				case 5: next = next_channel(pattern5); break;
				case 6: next = next_channel(pattern6); break;
				case 7: next = next_channel(pattern7); break;
				case 8: next = next_channel(pattern8); break;
				case 9: next = next_channel(pattern9); break;
				case 10: next = next_channel(patternA); break;
				case 11: next = next_channel(patternB); break;
				case 12: next = next_channel(patternC); break;
				case 13: next = next_channel(patternD); break;
				case 14: next = next_channel(patternE); break;
				case 15: next = next_channel(patternF); break;
			}

			return next;
		}

	private:
		static constexpr std::array<Channel, 2> pattern1 = { Channel::Write, Channel::None };
		static constexpr std::array<Channel, 2> pattern2 = { Channel::C, Channel::None };
		static constexpr std::array<Channel, 3> pattern3 = { Channel::C, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 3> pattern4 = { Channel::B, Channel::None, Channel::None };
		static constexpr std::array<Channel, 3> pattern5 = { Channel::B, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 3> pattern6 = { Channel::B, Channel::C, Channel::None };
		static constexpr std::array<Channel, 4> pattern7 = { Channel::B, Channel::C, Channel::Write, Channel::None };
		static constexpr std::array<Channel, 2> pattern8 = { Channel::A, Channel::None };
		static constexpr std::array<Channel, 2> pattern9 = { Channel::A, Channel::Write };
		static constexpr std::array<Channel, 2> patternA = { Channel::A, Channel::C };
		static constexpr std::array<Channel, 3> patternB = { Channel::A, Channel::C, Channel::Write };
		static constexpr std::array<Channel, 3> patternC = { Channel::A, Channel::B, Channel::None };
		static constexpr std::array<Channel, 3> patternD = { Channel::A, Channel::B, Channel::Write };
		static constexpr std::array<Channel, 3> patternE = { Channel::A, Channel::B, Channel::C };
		static constexpr std::array<Channel, 4> patternF = { Channel::A, Channel::B, Channel::C, Channel::Write };
		template <typename ArrayT> Channel next_channel(const ArrayT &list) {
			const Channel result = list[index_];
			index_ = (index_ + 1) % list.size();
			return result;
		}

		int control_ = 0;
		int index_ = 0;
		bool complete_ = false;
};

class Blitter: public DMADevice<4, 4> {
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
//		void set_vertical_size(uint16_t value);
//		void set_horizontal_size(uint16_t value);
		void set_data(int channel, uint16_t value);

		uint16_t get_status();

		bool advance_dma();

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

		bool not_zero_flag_ = false;
};

}


#endif /* Blitter_hpp */
