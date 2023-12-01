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
			next_access_low_ = true;
		}

		void mask_reset() {
			for(auto &channel : channels_) {
				channel.mask = false;
			}
		}

		void master_reset() {
			flip_flop_reset();
			for(auto &channel : channels_) {
				channel.mask = true;
				channel.transfer_complete = true;
				channel.request = false;
			}
		}

		template <int address>
		void write(uint8_t value) {
			constexpr int channel = (address >> 1) & 3;
			constexpr bool is_count = address & 1;

			next_access_low_ ^= true;
			if(next_access_low_) {
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

			next_access_low_ ^= true;
			if(next_access_low_) {
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

		void set_reset_mask(uint8_t value) {
			channels_[value & 3].mask = value & 4;
		}

		void set_reset_request(uint8_t value) {
			channels_[value & 3].request = value & 4;
		}

		void set_mask(uint8_t value) {
			channels_[0].mask = value & 1;
			channels_[1].mask = value & 2;
			channels_[2].mask = value & 4;
			channels_[3].mask = value & 8;
		}

		void set_mode(uint8_t value) {
			channels_[value & 3].transfer = Channel::Transfer((value >> 2) & 3);
			channels_[value & 3].autoinitialise = value & 0x10;
			channels_[value & 3].address_decrement = value & 0x20;
			channels_[value & 3].mode = Channel::Mode(value >> 6);
		}

		void set_command(uint8_t value) {
			enable_memory_to_memory_ = value & 0x01;
			enable_channel0_address_hold_ = value & 0x02;
			enable_controller_ = value & 0x04;
			compressed_timing_ = value & 0x08;
			rotating_priority_ = value & 0x10;
			extended_write_selection_ = value & 0x20;
			dreq_active_low_ = value & 0x40;
			dack_sense_active_high_ = value & 0x80;
		}

		uint8_t status() {
			const uint8_t result =
				(channels_[0].transfer_complete ? 0x01 : 0x00) |
				(channels_[1].transfer_complete ? 0x02 : 0x00) |
				(channels_[2].transfer_complete ? 0x04 : 0x00) |
				(channels_[3].transfer_complete ? 0x08 : 0x00) |

				(channels_[0].request ? 0x10 : 0x00) |
				(channels_[1].request ? 0x20 : 0x00) |
				(channels_[2].request ? 0x40 : 0x00) |
				(channels_[3].request ? 0x80 : 0x00);

			for(auto &channel : channels_) {
				channel.transfer_complete = false;
			}
			return result;
		}

	private:
		// Low/high byte latch.
		bool next_access_low_ = true;

		// Various fields set by the command register.
		bool enable_memory_to_memory_ = false;
		bool enable_channel0_address_hold_ = false;
		bool enable_controller_ = false;
		bool compressed_timing_ = false;
		bool rotating_priority_ = false;
		bool extended_write_selection_ = false;
		bool dreq_active_low_ = false;
		bool dack_sense_active_high_ = false;

		// Per-channel state.
		struct Channel {
			bool mask = false;
			enum class Transfer {
				Verify, Write, Read, Invalid
			} transfer = Transfer::Verify;
			bool autoinitialise = false;
			bool address_decrement = false;
			enum class Mode {
				Demand, Single, Block, Cascade
			} mode = Mode::Demand;

			bool request = false;
			bool transfer_complete = false;

			CPU::RegisterPair16 address, count;
		};
		std::array<Channel, 4> channels_;
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
