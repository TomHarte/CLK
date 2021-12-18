//
//  Chipset.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Chipset.hpp"

#ifndef NDEBUG
#define NDEBUG
#endif

#define LOG_PREFIX "[Amiga chipset] "
#include "../../Outputs/Log.hpp"

#include <algorithm>
#include <cassert>
#include <tuple>

using namespace Amiga;

namespace {

template <typename EnumT, EnumT... T> struct Mask {
	static constexpr uint16_t value = 0;
};

template <typename EnumT, EnumT F, EnumT... T> struct Mask<EnumT, F, T...> {
	static constexpr uint16_t value = uint16_t(F) | Mask<EnumT, T...>::value;
};

template <InterruptFlag... Flags> struct InterruptMask: Mask<InterruptFlag, Flags...> {};
template <DMAFlag... Flags> struct DMAMask: Mask<DMAFlag, Flags...> {};

}

#define DMA_CONSTRUCT *this, reinterpret_cast<uint16_t *>(map.chip_ram.data()), map.chip_ram.size() >> 1

Chipset::Chipset(MemoryMap &map, int input_clock_rate) :
	blitter_(DMA_CONSTRUCT),
	sprites_{
		Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT},
		Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT},	Sprite{DMA_CONSTRUCT}
	},
	bitplanes_(DMA_CONSTRUCT),
	copper_(DMA_CONSTRUCT),
	audio_(DMA_CONSTRUCT, float(input_clock_rate / 2.0)),
	crt_(908, 4, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
	cia_a_handler_(map, disk_controller_, mouse_),
	cia_b_handler_(disk_controller_),
	cia_a(cia_a_handler_),
	cia_b(cia_b_handler_),
	disk_(DMA_CONSTRUCT),
	disk_controller_(Cycles(input_clock_rate), *this, disk_, cia_b),
	keyboard_(cia_a.serial_input) {
	disk_controller_.set_clocking_hint_observer(this);

	joysticks_.emplace_back(new Joystick());
	cia_a_handler_.set_joystick(&joystick(0));

	// Very conservatively crop, to roughly the centre 88% of a frame.
	// This rectange was specifically calibrated around the default Workbench display.
	crt_.set_visible_area(Outputs::Display::Rect(0.05f, 0.055f, 0.88f, 0.88f));
}

#undef DMA_CONSTRUCT

Chipset::Changes Chipset::run_for(HalfCycles length) {
	return run<false>(length);
}

Chipset::Changes Chipset::run_until_after_cpu_slot() {
	return run<true>();
}

void Chipset::set_cia_interrupts(bool cia_a_interrupt, bool cia_b_interrupt) {
	// TODO: are these really latched, or are they active live?
	// If latched, is it only on a leading edge?
//	interrupt_requests_ &= ~InterruptMask<InterruptFlag::IOPortsAndTimers, InterruptFlag::External>::value;
	interrupt_requests_ |=
		(cia_a_interrupt ? InterruptMask<InterruptFlag::IOPortsAndTimers>::value : 0) |
		(cia_b_interrupt ? InterruptMask<InterruptFlag::External>::value : 0);
	update_interrupts();
}

void Chipset::posit_interrupt(InterruptFlag flag) {
	interrupt_requests_ |= uint16_t(flag);
	update_interrupts();
}

void DMADeviceBase::posit_interrupt(InterruptFlag flag) {
	chipset_.posit_interrupt(flag);
}

void Chipset::apply_ham(uint8_t modification) {
	uint8_t *const colour = reinterpret_cast<uint8_t *>(&last_colour_);

	// Allow for swizzled storage.
	switch(modification & 0x24) {
		case 0x00:	// Direct palette lookup.
			last_colour_ = swizzled_palette_[modification & 0x1b];
		break;
		case 0x04:	// Replace red.
			colour[0] = uint8_t(
				((modification & 0x10) >> 1) |	// bit 3.
				((modification & 0x02) << 1) |	// bit 2.
				((modification & 0x08) >> 2) |	// bit 1.
				(modification & 0x01)			// bit 0.
			);
		break;
		case 0x20:	// Replace blue.
			colour[1] = uint8_t(
				(colour[1] & 0xf0) |
				((modification & 0x10) >> 1) |	// bit 3.
				((modification & 0x02) << 1) |	// bit 2.
				((modification & 0x08) >> 2) |	// bit 1.
				(modification & 0x01)			// bit 0.
			);
		break;
		case 0x24:	// Replace green.
			colour[1] = uint8_t(
				(colour[1] & 0x0f) |
				((modification & 0x10) << 3) |	// bit 3.
				((modification & 0x02) << 5) |	// bit 2.
				((modification & 0x08) << 2) |	// bit 1.
				((modification & 0x01) << 4)	// bit 0.
			);
		break;
	}
}

void Chipset::output_pixels(int cycles_until_sync) {
	// Try to get a new buffer if none is currently allocated.
	if(!pixels_) {
		uint16_t *const new_pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(size_t(4 * cycles_until_sync)));
		if(new_pixels) {
			flush_output();
		}
		pixels_ = new_pixels;
	}

	// Get the next four playfield pixels (which, in low resolution mode, will
	// be repetitious — the playfield has been expanded as if in high res).
	const uint32_t playfield = bitplane_pixels_.get(is_high_res_);

	// Output playfield pixels, if a buffer was allocated.
	// TODO: HAM.
	if(pixels_) {
		if(hold_and_modify_) {
			apply_ham(uint8_t(playfield >> 16));
			pixels_[0] = pixels_[1] = last_colour_;

			apply_ham(uint8_t(playfield));
			pixels_[2] = pixels_[3] = last_colour_;
		} else if(dual_playfields_) {
			// Dense: just write both.
			// TODO: this could easily be just a table lookup, exactly as per swizzled_palette_.
			if(even_over_odd_) {
				pixels_[0] = palette_[8 + ((playfield >> 27) & 7)];
				pixels_[1] = palette_[8 + ((playfield >> 19) & 7)];
				pixels_[2] = palette_[8 + ((playfield >> 11) & 7)];
				pixels_[3] = palette_[8 + ((playfield >> 3) & 7)];

				if((playfield >> 24) & 7) pixels_[0] = palette_[(playfield >> 24) & 7];
				if((playfield >> 16) & 7) pixels_[1] = palette_[(playfield >> 16) & 7];
				if((playfield >> 8) & 7) pixels_[2] = palette_[(playfield >> 8) & 7];
				if(playfield & 7) pixels_[3] = palette_[playfield & 7];
			} else {
				pixels_[0] = palette_[(playfield >> 24) & 7];
				pixels_[1] = palette_[(playfield >> 16) & 7];
				pixels_[2] = palette_[(playfield >> 8) & 7];
				pixels_[3] = palette_[playfield & 7];

				if((playfield >> 27) & 7) pixels_[0] = palette_[8 + ((playfield >> 27) & 7)];
				if((playfield >> 19) & 7) pixels_[1] = palette_[8 + ((playfield >> 19) & 7)];
				if((playfield >> 11) & 7) pixels_[2] = palette_[8 + ((playfield >> 11) & 7)];
				if((playfield >> 3) & 7) pixels_[3] = palette_[8 + ((playfield >> 3) & 7)];
			}
		} else {
			pixels_[0] = swizzled_palette_[playfield >> 24];
			pixels_[1] = swizzled_palette_[(playfield >> 16) & 0xff];
			pixels_[2] = swizzled_palette_[(playfield >> 8) & 0xff];
			pixels_[3] = swizzled_palette_[playfield & 0xff];
		}
	}

	// Compute masks potentially to obscure sprites.
	int playfield_odd_pixel_mask =
		(((playfield >> 22) | (playfield >> 24) | (playfield >> 26)) & 8) |
		(((playfield >> 15) | (playfield >> 17) | (playfield >> 19)) & 4) |
		(((playfield >> 8) | (playfield >> 10) | (playfield >> 12)) & 2) |
		(((playfield >> 1) | (playfield >> 3) | (playfield >> 5)) & 1);
	int playfield_even_pixel_mask =
		(((playfield >> 21) | (playfield >> 23) | (playfield >> 25)) & 8) |
		(((playfield >> 14) | (playfield >> 16) | (playfield >> 18)) & 4) |
		(((playfield >> 7) | (playfield >> 9) | (playfield >> 11)) & 2) |
		(((playfield >> 0) | (playfield >> 2) | (playfield >> 4)) & 1);

	// If only a single playfield is in use, treat the mask as playing
	// into the priority selected for the even bitfields.
	if(!dual_playfields_) {
		playfield_even_pixel_mask |= playfield_odd_pixel_mask;
		playfield_odd_pixel_mask = 0;
	}

	// Process sprites.
	int collision_masks[4] = {0, 0, 0, 0};
	int index = int(sprite_shifters_.size());
	for(auto shifter = sprite_shifters_.rbegin(); shifter != sprite_shifters_.rend(); ++shifter) {
		// Update the index, and skip this shifter entirely if it's empty.
		--index;
		const uint8_t data = shifter->get();
		if(!data) continue;

		// Determine the collision mask.
		collision_masks[index] = data | (data >> 1);
		if(collisions_flags_ & (0x1000 << index)) {
			collision_masks[index] |= (data >> 2) | (data >> 3);
		}
		collision_masks[index] = (collision_masks[index] & 0x01) | ((collision_masks[index] & 0x10) >> 3);

		// Get the specific pixel mask.
		const int pixel_mask =
			(
				((odd_priority_ <= index) ? playfield_odd_pixel_mask : 0) |
				((even_priority_ <= index) ? playfield_even_pixel_mask : 0)
			);

		// Output pixels, if a buffer exists.
		const auto base = (index << 2) + 16;
		if(pixels_) {
			if(sprites_[size_t((index << 1) + 1)].attached) {
				// Left pixel.
				if(data >> 4) {
					if(!(pixel_mask & 0x8)) pixels_[0] = palette_[16 + (data >> 4)];
					if(!(pixel_mask & 0x4)) pixels_[1] = palette_[16 + (data >> 4)];
				}

				// Right pixel.
				if(data & 15) {
					if(!(pixel_mask & 0x2)) pixels_[2] = palette_[16 + (data & 15)];
					if(!(pixel_mask & 0x1)) pixels_[3] = palette_[16 + (data & 15)];
				}
			} else {
				// Left pixel.
				if((data >> 4) & 3) {
					if(!(pixel_mask & 0x8)) pixels_[0] = palette_[base + ((data >> 4)&3)];
					if(!(pixel_mask & 0x4)) pixels_[1] = palette_[base + ((data >> 4)&3)];
				}
				if(data >> 6) {
					if(!(pixel_mask & 0x8)) pixels_[0] = palette_[base + (data >> 6)];
					if(!(pixel_mask & 0x4)) pixels_[1] = palette_[base + (data >> 6)];
				}

				// Right pixel.
				if(data & 3) {
					if(!(pixel_mask & 0x2)) pixels_[2] = palette_[base + (data & 3)];
					if(!(pixel_mask & 0x1)) pixels_[3] = palette_[base + (data & 3)];
				}
				if((data >> 2) & 3) {
					if(!(pixel_mask & 0x2)) pixels_[2] = palette_[base + ((data >> 2)&3)];
					if(!(pixel_mask & 0x1)) pixels_[3] = palette_[base + ((data >> 2)&3)];
				}
			}
		}
	}

	// Compute playfield collision mask and populate collisions register.
	const uint32_t playfield_collisions = (playfield & playfield_collision_mask_) ^ playfield_collision_complement_;
	int playfield_collisions_mask =
		(playfield_collisions | (playfield_collisions >> 1) | (playfield_collisions >> 2)) & 0x09090909;
	playfield_collisions_mask =
		playfield_collisions_mask | (playfield_collisions_mask >> 8) | (playfield_collisions_mask >> 15) | (playfield_collisions_mask >> 22);
	const int playfield_collision_masks[2] = {
		playfield_collisions_mask,
		playfield_collisions_mask >> 3
	};

	// TODO: as below, but without conditionals...
	collisions_ |=
		((collision_masks[2] & collision_masks[3]) ? 0x4000 : 0x0000) |

		((collision_masks[1] & collision_masks[3]) ? 0x2000 : 0x0000) |
		((collision_masks[1] & collision_masks[2]) ? 0x1000 : 0x0000) |

		((collision_masks[0] & collision_masks[3]) ? 0x0800 : 0x0000) |
		((collision_masks[0] & collision_masks[2]) ? 0x0400 : 0x0000) |
		((collision_masks[0] & collision_masks[1]) ? 0x0200 : 0x0000) |

		((playfield_collision_masks[1] & collision_masks[3]) ? 0x0100 : 0x0000) |
		((playfield_collision_masks[1] & collision_masks[2]) ? 0x0080 : 0x0000) |
		((playfield_collision_masks[1] & collision_masks[1]) ? 0x0040 : 0x0000) |
		((playfield_collision_masks[1] & collision_masks[0]) ? 0x0020 : 0x0000) |

		((playfield_collision_masks[0] & collision_masks[3]) ? 0x0010 : 0x0000) |
		((playfield_collision_masks[0] & collision_masks[2]) ? 0x0008 : 0x0000) |
		((playfield_collision_masks[0] & collision_masks[1]) ? 0x0004 : 0x0000) |
		((playfield_collision_masks[0] & collision_masks[0]) ? 0x0002 : 0x0000) |

		((playfield_collision_masks[0] & playfield_collision_masks[1]) ? 0x0001 : 0x0000);

	// Advance pixel pointer (if applicable).
	if(pixels_) {
		pixels_ += 4;
	}
}

template <int cycle> void Chipset::output() {
	// Notes to self on guesses below:
	//
	// Hardware stop is at 0x18;
	// 12/64 * 227 = 42.5625
	//
	// "However, horizontal blanking actually limits the displayable
	// video to 368 low resolution pixel"
	//
	// => 184 windows out of 227 are visible, which concurs.

	// TODO: Reload bitplanes if anything is pending.
//	if(has_next_bitplanes_) {
//		has_next_bitplanes_ = false;
//		bitplane_pixels_.set(
//			previous_bitplanes_,
//			next_bitplanes_,
//			odd_delay_,
//			even_delay_
//		);
//		previous_bitplanes_ = next_bitplanes_;
//	}

	// Advance audio.
	audio_.output();

	// Trigger any sprite loads encountered.
	constexpr auto dcycle = cycle << 1;
	static_assert(std::tuple_size<decltype(sprites_)>::value % 2 == 0);
	for(size_t c = 0; c < sprites_.size(); c += 2) {
		if( sprites_[c].visible &&
			dcycle <= sprites_[c].h_start &&
			dcycle+2 > sprites_[c].h_start) {
			sprite_shifters_[c >> 1].load<0>(
				sprites_[c].data[1],
				sprites_[c].data[0],
				sprites_[c].h_start & 1);
		}

		if(	sprites_[c+1].visible &&
			dcycle <= sprites_[c + 1].h_start &&
			dcycle+2 > sprites_[c + 1].h_start) {
			sprite_shifters_[c >> 1].load<1>(
				sprites_[c + 1].data[1],
				sprites_[c + 1].data[0],
				sprites_[c + 1].h_start & 1);
		}
	}

	//
	// Horizontal sync: HC18–HC35;
	// Horizontal blank: HC15–HC53.
	//
	// Beyond that: guesswork.
	//
	// So, from cycle 0:
	//
	//	15 cycles border/pixels;
	//	3 cycles blank;
	//	17 cycles sync;
	//	3 cycles blank;
	//	9 cycles colour burst;
	//	6 cycles blank;
	//	then more border/pixels to end of line.
	//
	// (???)

	constexpr int end_of_pixels	= 15;
	constexpr int blank1		= 3 + end_of_pixels;
	constexpr int sync			= 17 + blank1;
	constexpr int blank2 		= 3 + sync;
	constexpr int burst 		= 9 + blank2;
	constexpr int blank3 		= 6 + burst;
	static_assert(blank3 == 53);

#define LINK(location, action, length)	\
	if(cycle == (location))	{			\
		crt_.action((length) * 4);		\
	}

	if(y_ < vertical_blank_height_) {
		if(!cycle) {
			flush_output();
		}

		// Put three lines of sync at the centre of the vertical blank period.
		// Offset by half a line if interlaced and on an odd frame.
		const int midline = vertical_blank_height_ >> 1;
		if(is_long_field_) {
			if(y_ < midline - 1 || y_ > midline + 2) {
				LINK(blank1, output_blank, blank1);
				LINK(sync, output_sync, sync - blank1);
				LINK(line_length_ - 1, output_blank, line_length_ - 1 - sync);
			} else if(y_ == midline - 1) {
				LINK(113, output_blank, 113);
				LINK(line_length_ - 1, output_sync, line_length_ - 1 - 113);
			} else if(y_ == midline + 2) {
				LINK(113, output_sync, 113);
				LINK(line_length_ - 1, output_blank, line_length_ - 1 - 113);
			} else {
				LINK(blank1, output_sync, blank1);
				LINK(sync, output_blank, sync - blank1);
				LINK(line_length_ - 1, output_sync, line_length_ - 1 - sync);
			}
		} else {
			if(y_ < midline - 1 || y_ > midline + 1) {
				LINK(blank1, output_blank, blank1);
				LINK(sync, output_sync, sync - blank1);
				LINK(line_length_ - 1, output_blank, line_length_ - 1 - sync);
			} else {
				LINK(blank1, output_sync, blank1);
				LINK(sync, output_blank, sync - blank1);
				LINK(line_length_ - 1, output_sync, line_length_ - 1 - sync);
			}
		}
	} else {
		// TODO: incorporate the lowest display window bits elsewhere.
		display_horizontal_ |= cycle == (display_window_start_[0] >> 1);
		display_horizontal_ &= cycle != (display_window_stop_[0] >> 1);

		if(cycle == end_of_pixels) {
			flush_output();
		}

		// Output the correct sequence of blanks, syncs and burst atomically.
		LINK(blank1, output_blank, blank1 - end_of_pixels);
		LINK(sync, output_sync, sync - blank1);
		LINK(blank2, output_blank, blank2 - sync);
		LINK(burst, output_default_colour_burst, burst - blank2);	// TODO: only if colour enabled.
		LINK(blank3, output_blank, blank3 - burst);

		if constexpr (cycle < end_of_pixels || cycle > blank3) {
			const bool is_pixel_display = display_horizontal_ && fetch_vertical_;

			if(
				(is_pixel_display == is_border_) ||
				(is_border_ && border_colour_ != palette_[0])) {
				flush_output();

				is_border_ = !is_pixel_display;
				border_colour_ = palette_[0];
			}

			if(is_pixel_display) {
				// This is factored out because it is fairly convoluted and is not a function of
				// the template parameter; I doubt I'm somehow being smarter than the optimising
				// compiler, but this makes my debugging life a lot easier and I don't imagine
				// the compiler will do a worse job.
				output_pixels(line_length_ + end_of_pixels - cycle);
			}
			++zone_duration_;
		}
	}
#undef LINK

	// Update all active pixel shifters.
	bitplane_pixels_.shift(is_high_res_);
	sprite_shifters_[0].shift();
	sprite_shifters_[1].shift();
	sprite_shifters_[2].shift();
	sprite_shifters_[3].shift();
}

void Chipset::flush_output() {
	if(!zone_duration_) return;

	if(is_border_) {
		uint16_t *const pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
		if(pixels) {
			*pixels = border_colour_;
		}
		crt_.output_data(zone_duration_ * 4, 1);
		last_colour_ = border_colour_;
	} else {
		crt_.output_data(zone_duration_ * 4);
	}
	zone_duration_ = 0;
	pixels_ = nullptr;
}

/// @returns @c true if this was a CPU slot; @c false otherwise.
template <int cycle, bool stop_if_cpu> bool Chipset::perform_cycle() {
	constexpr uint16_t AudioFlags[]	= {
		DMAMask<DMAFlag::AudioChannel0, DMAFlag::AllBelow>::value,
		DMAMask<DMAFlag::AudioChannel1, DMAFlag::AllBelow>::value,
		DMAMask<DMAFlag::AudioChannel2, DMAFlag::AllBelow>::value,
		DMAMask<DMAFlag::AudioChannel3, DMAFlag::AllBelow>::value,
	};
	constexpr auto BlitterFlag	= DMAMask<DMAFlag::Blitter, DMAFlag::AllBelow>::value;
	constexpr auto BitplaneFlag	= DMAMask<DMAFlag::Bitplane, DMAFlag::AllBelow>::value;
	constexpr auto CopperFlag	= DMAMask<DMAFlag::Copper, DMAFlag::AllBelow>::value;
	constexpr auto DiskFlag		= DMAMask<DMAFlag::Disk, DMAFlag::AllBelow>::value;
	constexpr auto SpritesFlag	= DMAMask<DMAFlag::Sprites, DMAFlag::AllBelow>::value;

	// Update state as to whether bitplane fetching should happen now.
	//
	// TODO: figure out how the hard stops factor into this.
	//

	// Top priority: bitplane collection.
	// TODO: mask off fetch_window_'s lower bits. (Dependant on high/low-res?)
	// Also: fetch_stop_ and that + 12/8 is the best I can discern from the Hardware Reference,
	// but very obviously isn't how the actual hardware works. Explore on that.
	fetch_horizontal_ |= cycle == fetch_window_[0];
	if(cycle == fetch_window_[1]) fetch_stop_ = cycle + (is_high_res_ ? 12 : 8);
	fetch_horizontal_ &= cycle != fetch_stop_;
	if((dma_control_ & BitplaneFlag) == BitplaneFlag) {
		if(fetch_vertical_ && fetch_horizontal_ && bitplanes_.advance_dma(cycle)) {
			did_fetch_ = true;
			return false;
		}
	}

	// Contradictory snippets from the Hardware Reference manual:
	//
	// 1)
	// The Copper is a two-cycle processor that requests the bus only during
	// odd-numbered memory cycles. This prevents collision with audio, disk,
	// refresh, sprites, and most low resolution display DMA access, all of which
	// use only the even-numbered memory cycles.
	//
	// 2)
	//  |<- - - - - - - - average 68000 cycle - - - - - - - - ->|
	//  |                                                       |
	//  |<- - - - internal  - - - ->|<- - - - - memory  - - - ->|
	//  |         operation         |           access          |
	//  |         portion           |           portion         |
	//  |                           |                           |
	//  |        odd cycle,         |         even cycle,       |
	//  |        assigned to        |         available to      |
	//  |        other devices      |         the 68000         |
	//
	//				Figure 6-10: Normal 68000 Cycle

	// There's also Figure 6-9, which in theory nails down slot usage, but
	// numbers the boundaries between slots rather than the slots themselves...
	// and has nine slots depicted between positions $20 and $21. So
	// whether the boundary numbers assign to the slots on their left or on
	// their right is entirely opaque.

	// I therefore take the word of Toni Wilen via https://eab.abime.net/showpost.php?p=938307&postcount=2
	// as definitive: "CPU ... generally ... uses even [cycles] only".
	//
	// So probably the Copper requests the bus only on _even_ cycles?


	// General rule:
	//
	//	Chipset work on odd cycles, 68000 access on even.
	//
	// Exceptions:
	//
	//	Bitplanes, the Blitter if a flag is set.

	if constexpr (cycle & 1) {
		// Odd slot use/priority:
		//
		//	1. Bitplane fetches [dealt with above].
		//	2. Refresh, disk, audio, sprites or Copper. Depending on region.
		//
		// Blitter and CPU priority is dealt with below.
		if constexpr (cycle >= 0x00 && cycle < 0x08) {
			// Memory refresh, four slots per line.
			return true;
		}

		if constexpr (cycle >= 0x08 && cycle < 0x0e) {
			if((dma_control_ & DiskFlag) == DiskFlag) {
				if(disk_.advance_dma()) {
					return false;
				}
			}
		}

		if constexpr (cycle >= 0xe && cycle < 0x16) {
			constexpr auto channel = (cycle - 0xe) >> 1;
			static_assert(channel >= 0 && channel < 4);
			static_assert(cycle != 0x15 || channel == 3);

			if((dma_control_ & AudioFlags[channel]) == AudioFlags[channel]) {
				if(audio_.advance_dma(channel)) {
					return false;
				}
			}
		}

		if constexpr (cycle >= 0x16 && cycle < 0x36) {
			if((dma_control_ & SpritesFlag) == SpritesFlag && y_ >= vertical_blank_height_) {
				constexpr auto sprite_id = (cycle - 0x16) >> 2;
				static_assert(sprite_id >= 0 && sprite_id < std::tuple_size<decltype(sprites_)>::value);

				if(sprites_[sprite_id].advance_dma(!(cycle&2))) {
					return false;
				}
			}
		}
	} else {
		// Bitplanes having been dealt with, specific even-cycle responsibility
		// is just possibly to pass to the Copper.
		//
		// The Blitter and CPU are dealt with outside of the odd/even test.
		if((dma_control_ & CopperFlag) == CopperFlag) {
			if(copper_.advance_dma(uint16_t(((y_ & 0xff) << 8) | cycle), blitter_.get_status())) {
				return false;
			}
		} else {
			copper_.stop();
		}
	}

	// Down here: give first refusal to the Blitter, otherwise
	// pass on to the CPU.
	return (dma_control_ & BlitterFlag) != BlitterFlag || !blitter_.advance_dma();
}

/// Performs all slots starting with @c first_slot and ending just before @c last_slot.
/// If @c stop_on_cpu is true, stops upon discovery of a CPU slot.
///
/// @returns the number of slots completed if @c stop_on_cpu was true and a CPU slot was found.
///			@c -1 otherwise.
template <bool stop_on_cpu> int Chipset::advance_slots(int first_slot, int last_slot) {
	if(first_slot == last_slot) {
		return -1;
	}
	assert(last_slot > first_slot);

#define C(x) 										\
	case x: 										\
		output<x>();								\
													\
		if constexpr (stop_on_cpu) {				\
			if(perform_cycle<x, stop_on_cpu>()) {	\
				return 1 + x - first_slot;			\
			}										\
		} else {									\
			perform_cycle<x, stop_on_cpu>(); 		\
		} 											\
													\
		if((x + 1) == last_slot) break;				\
		[[fallthrough]]

#define C10(x)	C(x); C(x+1); C(x+2); C(x+3); C(x+4); C(x+5); C(x+6); C(x+7); C(x+8); C(x+9);
	switch(first_slot) {
		C10(0);		C10(10);	C10(20);	C10(30);	C10(40);
		C10(50);	C10(60);	C10(70);	C10(80);	C10(90);
		C10(100);	C10(110);	C10(120);	C10(130);	C10(140);
		C10(150);	C10(160);	C10(170);	C10(180);	C10(190);
		C10(200);	C10(210);
		C(220);		C(221);		C(222);		C(223);		C(224);
		C(225);		C(226);		C(227);		C(228);

		default: assert(false);
	}
#undef C

	return -1;
}

template <bool stop_on_cpu> Chipset::Changes Chipset::run(HalfCycles length) {
	Changes changes;

	// This code uses 'pixels' as a measure, which is equivalent to one pixel clock time,
	// or half a cycle.
	auto pixels_remaining = length.as<int>();
	int hsyncs = 0, vsyncs = 0;

	// Update raster position, spooling out graphics.
	while(pixels_remaining) {
		// Determine number of pixels left on this line.
		const int line_pixels = std::min(pixels_remaining, (line_length_ * 4) - line_cycle_);

		const int start_slot = line_cycle_ >> 2;
		const int end_slot = (line_cycle_ + line_pixels) >> 2;
		const int actual_slots = advance_slots<stop_on_cpu>(start_slot, end_slot);

		if(stop_on_cpu && actual_slots >= 0) {
			// Run until the end of the named slot.
			if(actual_slots) {
				const int actual_line_pixels =
					(4 - (line_cycle_ & 3)) + ((actual_slots - 1) << 2);
				line_cycle_ += actual_line_pixels;
				changes.duration += HalfCycles(actual_line_pixels);
			}

			// Just ensure an exit.
			pixels_remaining = 0;
		} else {
			line_cycle_ += line_pixels;
			changes.duration += HalfCycles(line_pixels);
			pixels_remaining -= line_pixels;
		}

		// Advance intraline counter and possibly ripple upwards into
		// lines and fields.
		if(line_cycle_ == (line_length_ * 4)) {
			++hsyncs;

			line_cycle_ = 0;
			++y_;

			if(did_fetch_) {
				bitplanes_.do_end_of_line();
				previous_bitplanes_.clear();
			}
			did_fetch_ = false;
			fetch_horizontal_ = false;
			fetch_stop_ = 0xffff;

			if(y_ == short_field_height_ + is_long_field_) {
				++vsyncs;
				interrupt_requests_ |= InterruptMask<InterruptFlag::VerticalBlank>::value;
				update_interrupts();

				y_ = 0;

				// TODO: the manual is vague on when this happens. Try to find out.
				copper_.reload<0>();

				// Toggle next field length if interlaced.
				is_long_field_ ^= interlace_;
			}

			for(auto &sprite: sprites_) {
				sprite.advance_line(y_, y_ == vertical_blank_height_);
			}

			fetch_vertical_ |= y_ == display_window_start_[1];
			fetch_vertical_ &= y_ != display_window_stop_[1];
		}
		assert(line_cycle_ < line_length_ * 4);
	}

	// Advance the keyboard's serial output, at
	// close enough to 1,000,000 ticks/second.
	keyboard_divider_ += changes.duration;
	keyboard_.run_for(keyboard_divider_.divide(HalfCycles(14)));

	// The CIAs are on the E clock.
	cia_divider_ += changes.duration;
	const HalfCycles e_clocks = cia_divider_.divide(HalfCycles(20));
	if(e_clocks > HalfCycles(0)) {
		cia_a.run_for(e_clocks);
		cia_b.run_for(e_clocks);
	}

	// Propagate TOD updates to the CIAs, and feed their new interrupt
	// outputs back to here.
	cia_a.advance_tod(vsyncs);
	cia_b.advance_tod(hsyncs);
	set_cia_interrupts(cia_a.get_interrupt_line(), cia_b.get_interrupt_line());

	// Update the disk controller, if any drives are active.
	if(!disk_controller_is_sleeping_) {
		disk_controller_.run_for(changes.duration.cycles());
	}

	// Record the interrupt level.
	// TODO: is this useful?
	changes.interrupt_level = interrupt_level_;
	return changes;
}

void Chipset::post_bitplanes(const BitplaneData &data) {
	// For now this retains the storage that'll be used when I switch to
	// deferred loading, but continues to act as if the Amiga were barrel
	// shifting bitplane data.
	next_bitplanes_ = data;
	bitplane_pixels_.set(
		previous_bitplanes_,
		next_bitplanes_,
		odd_delay_,
		even_delay_
	);
	previous_bitplanes_ = next_bitplanes_;
}

void Chipset::update_interrupts() {
	audio_.set_interrupt_requests(interrupt_requests_);
	interrupt_level_ = 0;

	const uint16_t enabled_requests = interrupt_enable_ & interrupt_requests_ & 0x3fff;
	if(enabled_requests && (interrupt_enable_ & 0x4000)) {
		if(enabled_requests & InterruptMask<InterruptFlag::External>::value) {
			interrupt_level_ = 6;
		} else if(enabled_requests & InterruptMask<InterruptFlag::SerialPortReceive, InterruptFlag::DiskSyncMatch>::value) {
			interrupt_level_ = 5;
		} else if(enabled_requests & InterruptMask<InterruptFlag::AudioChannel0, InterruptFlag::AudioChannel1, InterruptFlag::AudioChannel2, InterruptFlag::AudioChannel3>::value) {
			interrupt_level_ = 4;
		} else if(enabled_requests & InterruptMask<InterruptFlag::Copper, InterruptFlag::VerticalBlank, InterruptFlag::Blitter>::value) {
			interrupt_level_ = 3;
		} else if(enabled_requests & InterruptMask<InterruptFlag::IOPortsAndTimers>::value) {
			interrupt_level_ = 2;
		} else if(enabled_requests & InterruptMask<InterruptFlag::SerialPortTransmit, InterruptFlag::DiskBlock, InterruptFlag::Software>::value) {
			interrupt_level_ = 1;
		}
	}
}

void Chipset::perform(const CPU::MC68000::Microcycle &cycle) {
	using Microcycle = CPU::MC68000::Microcycle;

	const uint32_t register_address = *cycle.address & ChipsetAddressMask;
	if(cycle.operation & Microcycle::Read) {
		cycle.set_value16(read<true>(register_address));
	} else {
		write<true>(register_address, cycle.value16());
	}
}

template <bool allow_conversion> void Chipset::write(uint32_t address, uint16_t value) {
#define ApplySetClear(target, mask)	{		\
	if(value & 0x8000) {					\
		target |= (value & mask);			\
	} else {								\
		target &= ~(value & mask);			\
	}										\
}

	switch(address) {
		default:
			// If there was nothing to write, perform a throwaway read.
			if constexpr (allow_conversion) read<false>(address);
		break;

		// Raster position.
		case 0x098:		// CLXCON
			collisions_flags_ = value;

			// Produce appropriate bitfield manipulation values, including shuffling the bits.
			playfield_collision_mask_ = bitplane_swizzle(uint32_t((collisions_flags_ & 0xfc0) >> 6));
			playfield_collision_complement_ = bitplane_swizzle(uint32_t((collisions_flags_ & 0x3f) ^ 0x3f));

			playfield_collision_mask_ |= (playfield_collision_mask_ << 8) | (playfield_collision_mask_ << 16) | (playfield_collision_mask_ << 24);
			playfield_collision_complement_ |= (playfield_collision_complement_ << 8) | (playfield_collision_complement_ << 16) | (playfield_collision_complement_ << 24);
		break;

		case 0x02a:		// VPOSW
			LOG("TODO: write vertical position high " << PADHEX(4) << cycle.value16());
		break;
		case 0x02c:		// VHPOSW
			LOG("TODO: write vertical position low " << PADHEX(4) << cycle.value16());
			is_long_field_ = value & 0x8000;
		break;

		// Joystick/mouse input.
		case 0x034:		// POTGO
//			LOG("TODO: pot port start");
		break;

		// Disk DMA and control.
		case 0x020:	disk_.set_pointer<0, 16>(value);	break;		// DSKPTH
		case 0x022:	disk_.set_pointer<0, 0>(value);		break;		// DSKPTL
		case 0x024:	disk_.set_length(value);			break;		// DSKLEN

		case 0x026:		// DSKDAT
			LOG("TODO: disk DMA; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case 0x09e:		// ADKCON
			LOG("Write disk control");
			ApplySetClear(paula_disk_control_, 0x7fff);

			disk_controller_.set_control(paula_disk_control_);
			disk_.set_control(paula_disk_control_);
			audio_.set_modulation_flags(paula_disk_control_);
		break;

		case 0x07e:		// DSKSYNC
			disk_controller_.set_sync_word(value);
		break;

		// Refresh.
		case 0x028:		// REFPTR
			LOG("TODO (maybe): refresh; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		// Serial port.
		case 0x030:		// SERDAT
			LOG("TODO: serial data: " << PADHEX(4) << cycle.value16());
		break;
		case 0x032:		// SERPER
			LOG("TODO: serial control: " << PADHEX(4) << cycle.value16());
			serial_.set_control(value);
		break;

		// DMA management.
		case 0x096:		// DMACON
			ApplySetClear(dma_control_, 0x1fff);
			audio_.set_channel_enables(dma_control_);
		break;

		// Interrupts.
		case 0x09a:		// INTENA
			ApplySetClear(interrupt_enable_, 0x7fff);
			update_interrupts();
		break;
		case 0x09c:		// INTREQ
			ApplySetClear(interrupt_requests_, 0x7fff);
			update_interrupts();
		break;

		// Display management.
		case 0x08e:		// DIWSTRT
			display_window_start_[0] = value & 0xff;
			display_window_start_[1] = value >> 8;
		break;
		case 0x090:	// DIWSTOP
			display_window_stop_[0] = 0x100 | (value & 0xff);
			display_window_stop_[1] = value >> 8;
			display_window_stop_[1] |= ((value >> 7) & 0x100) ^ 0x100;
		break;
		case 0x092:		// DDFSTRT
			if(fetch_window_[0] != value) {
				LOG("Fetch window start set to " << std::dec << cycle.value16());
			}
			fetch_window_[0] = value;
		break;
		case 0x094:		// DDFSTOP
			// TODO: something in my interpretation of ddfstart and ddfstop
			// means a + 8 is needed below for high-res displays. Investigate.
			if(fetch_window_[1] != value) {
				LOG("Fetch window stop set to " << std::dec << fetch_window_[1]);
			}
			fetch_window_[1] = value;
		break;

		// Bitplanes.
		case 0x0e0:	bitplanes_.set_pointer<0, 16>(value);	break;	// BPL1PTH
		case 0x0e2:	bitplanes_.set_pointer<0, 0>(value);	break;	// BPL1PTL
		case 0x0e4:	bitplanes_.set_pointer<1, 16>(value);	break;	// BPL2PTH
		case 0x0e6:	bitplanes_.set_pointer<1, 0>(value);	break;	// BPL2PTL
		case 0x0e8:	bitplanes_.set_pointer<2, 16>(value);	break;	// BPL3PTH
		case 0x0ea:	bitplanes_.set_pointer<2, 0>(value);	break;	// BPL3PTL
		case 0x0ec:	bitplanes_.set_pointer<3, 16>(value);	break;	// BPL4PTH
		case 0x0ee:	bitplanes_.set_pointer<3, 0>(value);	break;	// BPL4PTL
		case 0x0f0:	bitplanes_.set_pointer<4, 16>(value);	break;	// BPL5PTH
		case 0x0f2:	bitplanes_.set_pointer<4, 0>(value);	break;	// BPL5PTL
		case 0x0f4:	bitplanes_.set_pointer<5, 16>(value);	break;	// BPL6PTH
		case 0x0f6:	bitplanes_.set_pointer<5, 0>(value);	break;	// BPL6PTL

		case 0x100:	// BPLCON0
			bitplanes_.set_control(value);
			is_high_res_ = value & 0x8000;
			hold_and_modify_ = value & 0x0800;
			dual_playfields_ = value & 0x0400;
			interlace_ = value & 0x0004;
		break;
		case 0x102:	// BPLCON1
			odd_delay_ = value & 0x0f;
			even_delay_ = (value >> 4) & 0x0f;
		break;
		case 0x104:	// BPLCON2
			odd_priority_ = value & 7;				// i.e. "Playfield 1"; planes 1, 3 and 5.
			even_priority_ = (value >> 3) & 7;		// i.e. "Playfield 2"; planes 2, 4 and 6.
			even_over_odd_ = value & 0x40;
		break;

		case 0x106:		// BPLCON3 (ECS)
			LOG("TODO: Bitplane control; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case 0x108:	bitplanes_.set_modulo<0>(value);	break;	// BPL1MOD
		case 0x10a:	bitplanes_.set_modulo<1>(value);	break;	// BPL2MOD

		case 0x110:
		case 0x112:
		case 0x114:
		case 0x116:
		case 0x118:
		case 0x11a:
			LOG("TODO: Bitplane data; " << PADHEX(4) << value << " to " << *cycle.address);
		break;

		// Blitter.
		case 0x040:	blitter_.set_control(0, value);			break;
		case 0x042:	blitter_.set_control(1, value);			break;
		case 0x044:	blitter_.set_first_word_mask(value);	break;
		case 0x046:	blitter_.set_last_word_mask(value);		break;

		case 0x048:	blitter_.set_pointer<2, 16>(value);	break;
		case 0x04a:	blitter_.set_pointer<2, 0>(value);	break;
		case 0x04c:	blitter_.set_pointer<1, 16>(value);	break;
		case 0x04e:	blitter_.set_pointer<1, 0>(value);	break;
		case 0x050:	blitter_.set_pointer<0, 16>(value);	break;
		case 0x052:	blitter_.set_pointer<0, 0>(value);	break;
		case 0x054:	blitter_.set_pointer<3, 16>(value);	break;
		case 0x056:	blitter_.set_pointer<3, 0>(value);	break;

		case 0x058:	blitter_.set_size(value);			break;
		case 0x05a:	blitter_.set_minterms(value);		break;

		case 0x060:	blitter_.set_modulo<2>(value);		break;
		case 0x062:	blitter_.set_modulo<1>(value);		break;
		case 0x064:	blitter_.set_modulo<0>(value);		break;
		case 0x066:	blitter_.set_modulo<3>(value);		break;

		case 0x070:	blitter_.set_data(2, value);		break;
		case 0x072:	blitter_.set_data(1, value);		break;
		case 0x074:	blitter_.set_data(0, value);		break;

		// Audio.
#define Audio(index, pointer)	\
		case pointer + 0:	audio_.set_pointer<index, 16>(value);	break;	\
		case pointer + 2:	audio_.set_pointer<index, 0>(value);	break;	\
		case pointer + 4:	audio_.set_length(index, value);		break;	\
		case pointer + 6:	audio_.set_period(index, value);		break;	\
		case pointer + 8:	audio_.set_volume(index, value);		break;	\
		case pointer + 10:	audio_.set_data(index, value);		break;	\

		Audio(0, 0x0a0);
		Audio(1, 0x0b0);
		Audio(2, 0x0c0);
		Audio(3, 0x0d0);

#undef Audio

		// Copper.
		case 0x02e:	copper_.set_control(value);			break;	// COPCON
		case 0x080:	copper_.set_pointer<0, 16>(value);	break;	// COP1LCH
		case 0x082:	copper_.set_pointer<0, 0>(value);	break;	// COP1LCL
		case 0x084:	copper_.set_pointer<1, 16>(value);	break;	// COP2LCH
		case 0x086:	copper_.set_pointer<1, 0>(value);	break;	// COP2LCL
		case 0x088:	copper_.reload<0>();				break;
		case 0x08a:	copper_.reload<1>();				break;
		case 0x08c:
			LOG("TODO: coprocessor instruction fetch identity " << PADHEX(4) << value);
		break;

		// Sprites.
#define Sprite(index, pointer, position)	\
		case pointer + 0:	sprites_[index].set_pointer<0, 16>(value);		break;	\
		case pointer + 2:	sprites_[index].set_pointer<0, 0>(value);		break;	\
		case position + 0:	sprites_[index].set_start_position(value);		break;	\
		case position + 2:	sprites_[index].set_stop_and_control(value);	break;	\
		case position + 4:	sprites_[index].set_image_data(0, value);		break;	\
		case position + 6:	sprites_[index].set_image_data(1, value);		break;

		Sprite(0, 0x120, 0x140);
		Sprite(1, 0x124, 0x148);
		Sprite(2, 0x128, 0x150);
		Sprite(3, 0x12c, 0x158);
		Sprite(4, 0x130, 0x160);
		Sprite(5, 0x134, 0x168);
		Sprite(6, 0x138, 0x170);
		Sprite(7, 0x13c, 0x178);

#undef Sprite

		// Colour palette.
		case 0x180:	case 0x182:	case 0x184:	case 0x186:	case 0x188:	case 0x18a:	case 0x18c:	case 0x18e:
		case 0x190:	case 0x192:	case 0x194:	case 0x196:	case 0x198:	case 0x19a:	case 0x19c:	case 0x19e:
		case 0x1a0:	case 0x1a2:	case 0x1a4:	case 0x1a6:	case 0x1a8:	case 0x1aa:	case 0x1ac:	case 0x1ae:
		case 0x1b0:	case 0x1b2:	case 0x1b4:	case 0x1b6:	case 0x1b8:	case 0x1ba:	case 0x1bc:	case 0x1be: {
			// Store once in regular, linear order.
			const auto entry_address = (address - 0x180) >> 1;
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[entry_address]);
			entry[0] = value >> 8;
			entry[1] = value & 0xff;

			// Also store in bit-swizzled order. In this array,
			// instead of being indexed as [b4 b3 b2 b1 b0], index
			// as [b3 b1 b4 b2 b0], and include a second set of the
			// 32 colours, stored as half-bright.
			const auto swizzled_address = bitplane_swizzle(entry_address & 0x1f);
			uint8_t *const swizzled_entry = reinterpret_cast<uint8_t *>(&swizzled_palette_[swizzled_address]);
			swizzled_entry[0] = value >> 8;
			swizzled_entry[1] = value & 0xff;

			swizzled_entry[64] = (swizzled_entry[0] >> 1) & 0x77;
			swizzled_entry[65] = (swizzled_entry[1] >> 1) & 0x77;
		} break;
	}

#undef ApplySetClear
}

template <bool allow_conversion> uint16_t Chipset::read(uint32_t address) {
	switch(address) {
		default:
			// If there was nothing to read, perform a write.
			// TODO: Rather than 0xffff, should be whatever is left on the bus, vapour-lock style.
			if constexpr (allow_conversion) write<false>(address, 0xffff);
		return 0xffff;

		// Raster position.
		case 0x004: {		// VPOSR; b15 = LOF, b0 = b8 of y position.
			const uint16_t position = uint16_t(y_ >> 8);
			return
				position |
				(is_long_field_ ? 0x8000 : 0x0000);

			// b8–b14 should be:
			//	00 for PAL Agnus or fat Agnus
			//	10 for NTSC Agnus or fat Agnus
			//	20 for PAL high-res
			//	30 for NTSC high-res
		}
		case 0x006: {		// VHPOSR; b0–b7 = horizontal; b8–b15 = low bits of vertical position.
			const uint16_t position = uint16_t(((line_cycle_ >> 1) & 0x00ff) | (y_ << 8));
			return position;
		}

		case 0x00e: {		// CLXDAT
			const uint16_t result = collisions_;
			collisions_ = 0;
			return result;
		};

		// Joystick/mouse input.
		case 0x00a:	return mouse_.get_position();			// JOY0DAT
		case 0x00c:	return joystick(0).get_position();		// JOY1DAT

		case 0x016:		// POTGOR / POTINP
//			LOG("TODO: pot port read");
		return 0xff00;

		// Disk DMA and control.
		case 0x010:		// ADKCONR
			LOG("Read disk control");
		return paula_disk_control_;
		case 0x01a:		// DSKBYTR
			LOG("TODO: disk status");
			assert(false);	// Not yet implemented.
		return 0xffff;

		// Serial port.
		case 0x018:		// SERDATR
			LOG("TODO: serial data and status");
		return 0x3000;	// i.e. transmit buffer empty.

		// DMA management.
		case 0x002:	return dma_control_ | blitter_.get_status();		// DMACONR

		// Interrupts.
		case 0x01c:	return interrupt_enable_;							// INTENAR
		case 0x01e:	return interrupt_requests_;							// INTREQR
	}
}

// MARK: - CRT connection.

void Chipset::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Chipset::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

void Chipset::set_display_type(Outputs::Display::DisplayType type) {
	crt_.set_display_type(type);
}

Outputs::Display::DisplayType Chipset::get_display_type() const {
	return crt_.get_display_type();
}

// MARK: - CIA A.

Chipset::CIAAHandler::CIAAHandler(MemoryMap &map, DiskController &controller, Mouse &mouse) :
	map_(map), controller_(controller), mouse_(mouse) {}

void Chipset::CIAAHandler::set_port_output(MOS::MOS6526::Port port, uint8_t value) {
	if(port) {
		// CIA A, Port B: Parallel port output.
		LOG("TODO: parallel output " << PADHEX(2) << +value);
	} else {
		// CIA A, Port A:
		//
		//	b7:	/FIR1
		//	b6:	/FIR0
		//	b5:	/RDY
		//	b4:	/TRK0
		//	b3:	/WPRO
		//	b2:	/CHNG
		//	b1:	/LED		[output]
		//	b0:	OVL			[output]

		if(observer_) {
			observer_->set_led_status(led_name, !(value & 2));
		}
		map_.set_overlay(value & 1);
	}
}

uint8_t Chipset::CIAAHandler::get_port_input(MOS::MOS6526::Port port) {
	if(port) {
		LOG("TODO: parallel input?");
	} else {
		// Use the mouse as FIR0, the joystick as FIR1.
		return
			controller_.get_rdy_trk0_wpro_chng() &
			mouse_.get_cia_button() &
			(1 | (joystick_->get_cia_button() << 1));
	}
	return 0xff;
}

void Chipset::CIAAHandler::set_activity_observer(Activity::Observer *observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led(led_name, Activity::Observer::LEDPresentation::Persistent);
	}
}

// MARK: - CIA B.

Chipset::CIABHandler::CIABHandler(DiskController &controller) : controller_(controller) {}

void Chipset::CIABHandler::set_port_output(MOS::MOS6526::Port port, uint8_t value) {
	if(port) {
		// CIA B, Port B:
		//
		// Disk motor control, drive and head selection,
		// and stepper control:
		controller_.set_mtr_sel_side_dir_step(value);
	} else {
		// CIA B, Port A: Serial port control.
		//
		// b7: /DTR
		// b6: /RTS
		// b5: /CD
		// b4: /CTS
		// b3: /DSR
		// b2: SEL
		// b1: POUT
		// b0: BUSY
		LOG("TODO: DTR/RTS/etc: " << PADHEX(2) << +value);
	}
}

uint8_t Chipset::CIABHandler::get_port_input(MOS::MOS6526::Port) {
	LOG("Unexpected: input for CIA B");
	return 0xff;
}

// MARK: - ClockingHintObserver.

void Chipset::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference preference) {
	disk_controller_is_sleeping_ = preference == ClockingHint::Preference::None;
}

// MARK: - Synchronisation.

void Chipset::flush() {
}
