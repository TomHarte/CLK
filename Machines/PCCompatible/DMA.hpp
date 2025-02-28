//
//  DMA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Numeric/RegisterSizes.hpp"

#include "Memory.hpp"

#include <array>

namespace PCCompatible {

enum class AccessResult {
	Accepted,
	AcceptedWithEOP,
	NotAccepted,
};

class i8237 {
	public:
		//
		// CPU-facing interface.
		//

		template <int address>
		void write(uint8_t value) {
//			printf("DMA: Write %02x to %d\n", value, address);

			switch(address) {
				default: {
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
				} break;
				case 0x8:	set_command(value);			break;
				case 0x9:	set_reset_request(value);	break;
				case 0xa:	set_reset_mask(value);		break;
				case 0xb:	set_mode(value);			break;
				case 0xc:	flip_flop_reset();			break;
				case 0xd:	master_reset();				break;
				case 0xe:	mask_reset();				break;
				case 0xf:	set_mask(value);			break;
			}
		}

		template <int address>
		uint8_t read() {
//			printf("DMA: Read %d\n", address);
			switch(address) {
				default: {
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
				 } break;
				 case 0x8:	return status();				break;
				 case 0xd:	return temporary_register();	break;
			}
		}

		//
		// Interface for reading/writing via DMA.
		//

		/// Provides the next target address for @c channel if performing either a write (if @c is_write is @c true) or read (otherwise).
		///
		/// @returns A combined address and @c AccessResult.
		std::pair<uint16_t, AccessResult> access(size_t channel, bool is_write) {
			if(is_write && channels_[channel].transfer != Channel::Transfer::Write) {
				return std::make_pair(0, AccessResult::NotAccepted);
			}
			if(!is_write && channels_[channel].transfer != Channel::Transfer::Read) {
				return std::make_pair(0, AccessResult::NotAccepted);
			}

			const auto address = channels_[channel].address.full;
			channels_[channel].address.full += channels_[channel].address_decrement ? -1 : 1;

			--channels_[channel].count.full;

			const bool was_complete = channels_[channel].transfer_complete;
			channels_[channel].transfer_complete = (channels_[channel].count.full == 0xffff);
			if(channels_[channel].transfer_complete) {
				// TODO: _something_ with mode.
			}

			auto result = AccessResult::Accepted;
			if(!was_complete && channels_[channel].transfer_complete) {
				result = AccessResult::AcceptedWithEOP;
			}
			return std::make_pair(address, result);
		}

		void set_complete(size_t channel) {
			channels_[channel].transfer_complete = true;
		}

	private:
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

//			printf("DMA: status is %02x\n", result);

			return result;
		}

		uint8_t temporary_register() const {
			// Not actually implemented, so...
			return 0xff;
		}

		void flip_flop_reset() {
//			printf("DMA: Flip flop reset\n");
			next_access_low_ = true;
		}

		void mask_reset() {
//			printf("DMA: Mask reset\n");
			for(auto &channel : channels_) {
				channel.mask = false;
			}
		}

		void master_reset() {
//			printf("DMA: Master reset\n");
			flip_flop_reset();
			for(auto &channel : channels_) {
				channel.mask = true;
				channel.transfer_complete = false;
				channel.request = false;
			}

			// This is a bit of a hack; DMA channel 0 is supposed to be linked to the PIT,
			// performing DRAM refresh. It isn't yet. So hack this, and hack that.
			channels_[0].transfer_complete = true;
		}

		void set_reset_mask(uint8_t value) {
//			printf("DMA: Set/reset mask %02x\n", value);
			channels_[value & 3].mask = value & 4;
		}

		void set_reset_request(uint8_t value) {
//			printf("DMA: Set/reset request %02x\n", value);
			channels_[value & 3].request = value & 4;
		}

		void set_mask(uint8_t value) {
//			printf("DMA: Set mask %02x\n", value);
			channels_[0].mask = value & 1;
			channels_[1].mask = value & 2;
			channels_[2].mask = value & 4;
			channels_[3].mask = value & 8;
		}

		void set_mode(uint8_t value) {
//			printf("DMA: Set mode %02x\n", value);
			channels_[value & 3].transfer = Channel::Transfer((value >> 2) & 3);
			channels_[value & 3].autoinitialise = value & 0x10;
			channels_[value & 3].address_decrement = value & 0x20;
			channels_[value & 3].mode = Channel::Mode(value >> 6);
		}

		void set_command(uint8_t value) {
//			printf("DMA: Set command %02x\n", value);
			enable_memory_to_memory_ = value & 0x01;
			enable_channel0_address_hold_ = value & 0x02;
			enable_controller_ = value & 0x04;
			compressed_timing_ = value & 0x08;
			rotating_priority_ = value & 0x10;
			extended_write_selection_ = value & 0x20;
			dreq_active_low_ = value & 0x40;
			dack_sense_active_high_ = value & 0x80;
		}

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

		uint8_t channel_page(size_t channel) {
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

class DMA {
	public:
		i8237 controller;
		DMAPages pages;

		// Memory is set posthoc to resolve a startup time.
		void set_memory(Memory *memory) {
			memory_ = memory;
		}

		// TODO: this permits only 8-bit DMA. Fix that.
		AccessResult write(size_t channel, uint8_t value) {
			auto access = controller.access(channel, true);
			if(access.second == AccessResult::NotAccepted) {
				return access.second;
			}

			const uint32_t address = uint32_t(pages.channel_page(channel) << 16) | access.first;
			*memory_->at(address) = value;
			return access.second;
		}

	private:
		Memory *memory_;
};

}
