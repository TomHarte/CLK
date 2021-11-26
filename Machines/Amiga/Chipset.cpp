//
//  Chipset.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Chipset.hpp"

//#ifndef NDEBUG
//#define NDEBUG
//#endif

#define LOG_PREFIX "[Amiga chipset] "
#include "../../Outputs/Log.hpp"

#include <algorithm>
#include <cassert>

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

/// Expands @c source so that b7 is the least-significant bit of the most-significant byte of the result,
/// b6 is the least-significant bit of the next most significant byte, etc. b0 stays in place.
constexpr uint64_t expand_bitplane_byte(uint8_t source) {
	uint64_t result = source;									// 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 abcd efgh
	result = (result | (result << 28)) & 0x0000'000f'0000'000f;	// 0000 0000 0000 0000 0000 0000 0000 abcd 0000 0000 0000 0000 0000 0000 0000 efgh
	result = (result | (result << 14)) & 0x0003'0003'0003'0003;	// 0000 0000 0000 00ab 0000 0000 0000 00cd 0000 0000 0000 00ef 0000 0000 0000 00gh
	result = (result | (result << 7)) & 0x0101'0101'0101'0101;	// 0000 000a 0000 000b 0000 000c 0000 000d 0000 000e 0000 000f 0000 000g 0000 000h
	return result;
}

// A very small selection of test cases.
static_assert(expand_bitplane_byte(0xff) == 0x01'01'01'01'01'01'01'01);
static_assert(expand_bitplane_byte(0x55) == 0x00'01'00'01'00'01'00'01);
static_assert(expand_bitplane_byte(0xaa) == 0x01'00'01'00'01'00'01'00);
static_assert(expand_bitplane_byte(0x00) == 0x00'00'00'00'00'00'00'00);

}

#define DMA_CONSTRUCT *this, reinterpret_cast<uint16_t *>(map.chip_ram.data()), map.chip_ram.size() >> 1

Chipset::Chipset(MemoryMap &map, int input_clock_rate) :
	blitter_(DMA_CONSTRUCT),
	sprites_{
		{DMA_CONSTRUCT},	{DMA_CONSTRUCT},	{DMA_CONSTRUCT},	{DMA_CONSTRUCT},
		{DMA_CONSTRUCT},	{DMA_CONSTRUCT},	{DMA_CONSTRUCT},	{DMA_CONSTRUCT}
	},
	bitplanes_(DMA_CONSTRUCT),
	copper_(DMA_CONSTRUCT),
	audio_(DMA_CONSTRUCT, input_clock_rate),
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

Chipset::Changes Chipset::run_until_cpu_slot() {
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
	//

	// Advance audio.
	audio_.output();

	// Trigger any sprite loads encountered.
	constexpr auto dcycle = cycle << 1;
	for(int c = 0; c < 8; c += 2) {
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
				if(!pixels_) {
					uint16_t *const new_pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(4 * size_t(line_length_ + end_of_pixels - cycle)));
					if(new_pixels) {
						flush_output();
					}
					pixels_ = new_pixels;
				}

				if(pixels_) {
					// TODO:
					//
					//	(1) dump bit 5 of a six-bitplane playfield if this Chipset doesn't support extra half-bright.
					//	(2) priorities, in general;
					//	(3) collisions;
					//	(4) a much less dense implementation. In particular: map from [1–6]-bit input to present or
					//		absent flags, use those to update collisions and then to index a mask table for compositing.
					//		Which would mean getting through the whole ordeal without branching... as soon as I come up
					//		with a branchless method for priorities. Hmmmm.
					const uint32_t source = bitplane_pixels_.get(is_high_res_);

					// TODO:
					if(dual_playfields_) {
						// Dense: just write both.
						if(even_over_odd_) {
							pixels_[0] = palette_[8 + (source >> 27) & 7];
							pixels_[1] = palette_[8 + (source >> 19) & 7];
							pixels_[2] = palette_[8 + (source >> 11) & 7];
							pixels_[3] = palette_[8 + (source >> 3) & 7];

							if((source >> 24) & 7) pixels_[0] = palette_[(source >> 24) & 7];
							if((source >> 16) & 7) pixels_[1] = palette_[(source >> 16) & 7];
							if((source >> 8) & 7) pixels_[2] = palette_[(source >> 8) & 7];
							if(source & 7) pixels_[3] = palette_[source & 7];
						} else {
							pixels_[0] = palette_[(source >> 24) & 7];
							pixels_[1] = palette_[(source >> 16) & 7];
							pixels_[2] = palette_[(source >> 8) & 7];
							pixels_[3] = palette_[source & 7];

							if((source >> 27) & 7) pixels_[0] = palette_[8 + (source >> 27) & 7];
							if((source >> 19) & 7) pixels_[1] = palette_[8 + (source >> 19) & 7];
							if((source >> 11) & 7) pixels_[2] = palette_[8 + (source >> 11) & 7];
							if((source >> 3) & 7) pixels_[3] = palette_[8 + (source >> 3) & 7];
						}
					} else {
						pixels_[0] = swizzled_palette_[source >> 24];
						pixels_[1] = swizzled_palette_[(source >> 16) & 0xff];
						pixels_[2] = swizzled_palette_[(source >> 8) & 0xff];
						pixels_[3] = swizzled_palette_[source & 0xff];
					}

					for(int c = 3; c >= 0; --c) {
						const auto data = sprite_shifters_[c].get();
						if(!data) continue;
						const int base = (c << 2) + 16;

						if(sprites_[(c << 1) + 1].attached) {
							// Left pixel.
							if(data >> 4) {
								pixels_[0] = pixels_[1] = palette_[16 + (data >> 4)];
							}

							// Right pixel.
							if(data & 15) {
								pixels_[2] = pixels_[3] = palette_[16 + (data & 15)];
							}
						} else {
							// Left pixel.
							if((data >> 4) & 3) {
								pixels_[0] = pixels_[1] = palette_[base + ((data >> 4)&3)];
							}
							if(data >> 6) {
								pixels_[0] = pixels_[1] = palette_[base + (data >> 6)];
							}

							// Right pixel.
							if(data & 3) {
								pixels_[2] = pixels_[3] = palette_[base + (data & 3)];
							}
							if((data >> 2) & 3) {
								pixels_[2] = pixels_[3] = palette_[base + ((data >> 2)&3)];
							}
						}
					}

					pixels_ += 4;
				}
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

	// Reload if anything is pending.
	if(has_next_bitplanes_) {
		has_next_bitplanes_ = false;
		bitplane_pixels_.set(
			previous_bitplanes_,
			next_bitplanes_,
			odd_delay_,
			even_delay_
		);
		previous_bitplanes_ = next_bitplanes_;
	}
}

void Chipset::flush_output() {
	if(!zone_duration_) return;

	if(is_border_) {
		uint16_t *const pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
		if(pixels) {
			*pixels = border_colour_;
		}
		crt_.output_data(zone_duration_ * 4, 1);
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
		// TODO: offer a cycle for bitplane collection.
		// Probably need to indicate odd or even?
		if(fetch_vertical_ && fetch_horizontal_ && bitplanes_.advance_dma(cycle)) {
			did_fetch_ = true;
			return false;
		}
	}

	if constexpr (cycle & 1) {
		// Odd slot use/priority:
		//
		//	1. Bitplane fetches [dealt with above].
		//	2. Refresh, disk, audio, or sprites. Depending on region.
		//
		// Blitter and CPU priority is dealt with below.
		if constexpr (cycle >= 0x07 && cycle < 0x0d) {
			if((dma_control_ & DiskFlag) == DiskFlag) {
				if(disk_.advance_dma()) {
					return false;
				}
			}
		}

		if constexpr (cycle >= 0xd && cycle < 0x14) {
			constexpr auto channel = (cycle - 0xd) >> 1;
			static_assert(channel >= 0 && channel < 4);

			if((dma_control_ & AudioFlags[channel]) == AudioFlags[channel]) {
				if(audio_.advance_dma(channel)) {
					return false;
				}
			}
		}

		if constexpr (cycle >= 0x15 && cycle < 0x35) {
			if((dma_control_ & SpritesFlag) == SpritesFlag && y_ >= vertical_blank_height_) {
				constexpr auto sprite_id = (cycle - 0x15) >> 2;
				static_assert(sprite_id >= 0 && sprite_id < 8);

				if(sprites_[sprite_id].advance_dma(cycle&2)) {
					return false;
				}
			}
		}
	} else {
		// Bitplanes being dealt with, specific odd-cycle responsibility
		// is just possibly to pass to the Copper.
		//
		// The Blitter and CPU are dealt with outside of the odd/even test.
		if((dma_control_ & CopperFlag) == CopperFlag) {
			if(copper_.advance_dma(uint16_t(((y_ & 0xff) << 8) | (cycle & 0xfe)))) {
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

#define C(x) \
	case x: \
		if constexpr(stop_on_cpu) {\
			if(perform_cycle<x, stop_on_cpu>()) {\
				return x - first_slot;\
			}\
		} else {\
			perform_cycle<x, stop_on_cpu>(); \
		} \
		output<x>();	\
		if((x + 1) == last_slot) break;	\
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

			fetch_vertical_ |= y_ == display_window_start_[1];
			fetch_vertical_ &= y_ != display_window_stop_[1];

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

			for(int c = 0; c < 8; c++) {
				sprites_[c].advance_line(y_, y_ == vertical_blank_height_);
			}
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
	// Posted bitplanes should be received at the end of the
	// current memory slot. So put them aside for now, and
	// deal with them momentarily.
	has_next_bitplanes_ = true;
	next_bitplanes_ = data;
}

void Chipset::BitplaneShifter::set(const BitplaneData &previous, const BitplaneData &next, int odd_delay, int even_delay) {
	const uint16_t planes[6] = {
		uint16_t(((previous[0] << 16) | next[0]) >> even_delay),
		uint16_t(((previous[1] << 16) | next[1]) >> odd_delay),
		uint16_t(((previous[2] << 16) | next[2]) >> even_delay),
		uint16_t(((previous[3] << 16) | next[3]) >> odd_delay),
		uint16_t(((previous[4] << 16) | next[4]) >> even_delay),
		uint16_t(((previous[5] << 16) | next[5]) >> odd_delay),
	};

	// Swizzle bits into the form:
	//
	//	[b5 b3 b1 b4 b2 b0]
	//
	// ... and assume a suitably adjusted palette is in use elsewhere.
	// This makes dual playfields very easy to separate.
	data_[0] =
		(expand_bitplane_byte(uint8_t(planes[0])) << 0) |
		(expand_bitplane_byte(uint8_t(planes[2])) << 1) |
		(expand_bitplane_byte(uint8_t(planes[4])) << 2) |
		(expand_bitplane_byte(uint8_t(planes[1])) << 3) |
		(expand_bitplane_byte(uint8_t(planes[3])) << 4) |
		(expand_bitplane_byte(uint8_t(planes[5])) << 5);

	data_[1] =
		(expand_bitplane_byte(uint8_t(planes[0] >> 8)) << 0) |
		(expand_bitplane_byte(uint8_t(planes[2] >> 8)) << 1) |
		(expand_bitplane_byte(uint8_t(planes[4] >> 8)) << 2) |
		(expand_bitplane_byte(uint8_t(planes[1] >> 8)) << 3) |
		(expand_bitplane_byte(uint8_t(planes[3] >> 8)) << 4) |
		(expand_bitplane_byte(uint8_t(planes[5] >> 8)) << 5);
}

void Chipset::update_interrupts() {
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

#define RW(address)		address | ((cycle.operation & Microcycle::Read) << 12)
#define Read(address)	address | (Microcycle::Read << 12)
#define Write(address)	address

#define ApplySetClear(target, mask)	{		\
	const uint16_t value = cycle.value16();	\
	if(value & 0x8000) {					\
		target |= (value & mask);			\
	} else {								\
		target &= ~(value & mask);			\
	}										\
}

	const uint32_t register_address = *cycle.address & 0x1fe;
	switch(RW(register_address)) {
		default:
			LOG("Unimplemented chipset " << (cycle.operation & Microcycle::Read ? "read" : "write") <<  " " << PADHEX(6) << *cycle.address);
			if(cycle.operation & Microcycle::Read) {
				cycle.set_value16(0xffff);
			}
		break;

		// Raster position.
		case Read(0x004): {		// VPOSR; b15 = LOF, b0 = b8 of y position.
			const uint16_t position = uint16_t(y_ >> 8);
			cycle.set_value16(
				position |
				(is_long_field_ ? 0x8000 : 0x0000)
			);

			// b8–b14 should be:
			//	00 for PAL Agnus or fat Agnus
			//	10 for NTSC Agnus or fat Agnus
			//	20 for PAL high-res
			//	30 for NTSC high-res
		} break;
		case Read(0x006): {		// VHPOSR; b0–b7 = horizontal; b8–b15 = low bits of vertical position.
			const uint16_t position = uint16_t(((line_cycle_ >> 1) & 0x00ff) | (y_ << 8));
			cycle.set_value16(position);
		} break;

		case Write(0x02a):		// VPOSW
			LOG("TODO: write vertical position high " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x02c): {	// VHPOSW
			LOG("TODO: write vertical position low " << PADHEX(4) << cycle.value16());

			const uint16_t value = cycle.value16();
			is_long_field_ = value & 0x8000;
		} break;

		// Joystick/mouse input.
		case Read(0x00a):		// JOY0DAT
			cycle.set_value16(mouse_.get_position());
		break;
		case Read(0x00c):		// JOY1DAT
			cycle.set_value16(joystick(0).get_position());
		break;

		case Write(0x034):		// POTGO
//			LOG("TODO: pot port start");
		break;
		case Read(0x016):		// POTGOR / POTINP
//			LOG("TODO: pot port read");
			cycle.set_value16(0xff00);
		break;

		// Disk DMA and control.
		case Write(0x020):	disk_.set_pointer<0, 16>(cycle.value16());	break;		// DSKPTH
		case Write(0x022):	disk_.set_pointer<0, 0>(cycle.value16());	break;		// DSKPTL
		case Write(0x024):	disk_.set_length(cycle.value16());			break;		// DSKLEN

		case Write(0x026):		// DSKDAT
			LOG("TODO: disk DMA; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case Write(0x09e):		// ADKCON
			LOG("Write disk control");
			ApplySetClear(paula_disk_control_, 0x7fff);

			disk_controller_.set_control(paula_disk_control_);
			disk_.set_control(paula_disk_control_);
			audio_.set_modulation_flags(paula_disk_control_);
		break;
		case Read(0x010):		// ADKCONR
			LOG("Read disk control");
			cycle.set_value16(paula_disk_control_);
		break;

		case Write(0x07e):		// DSKSYNC
			disk_controller_.set_sync_word(cycle.value16());
		break;
		case Read(0x01a):		// DSKBYTR
			LOG("TODO: disk status");
			assert(false);	// Not yet implemented.
		break;

		// Refresh.
		case Write(0x028):		// REFPTR
			LOG("TODO (maybe): refresh; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		// Serial port.
		case Read(0x018):		// SERDATR
			LOG("TODO: serial data and status");
			cycle.set_value16(0x3000);	// i.e. transmit buffer empty.
		break;
		case Write(0x030):		// SERDAT
			LOG("TODO: serial data: " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x032):		// SERPER
			LOG("TODO: serial control: " << PADHEX(4) << cycle.value16());
			serial_.set_control(cycle.value16());
		break;

		// DMA management.
		case Read(0x002):		// DMACONR
			cycle.set_value16(dma_control_ | blitter_.get_status());
		break;
		case Write(0x096):		// DMACON
			ApplySetClear(dma_control_, 0x1fff);
			audio_.set_channel_enables(dma_control_);
		break;

		// Interrupts.
		case Write(0x09a):		// INTENA
			ApplySetClear(interrupt_enable_, 0x7fff);
			update_interrupts();
		break;
		case Read(0x01c):		// INTENAR
			cycle.set_value16(interrupt_enable_);
		break;

		case Write(0x09c):		// INTREQ
			ApplySetClear(interrupt_requests_, 0x7fff);
			update_interrupts();
			audio_.set_interrupt_requests(interrupt_requests_);
		break;
		case Read(0x01e):		// INTREQR
			cycle.set_value16(interrupt_requests_);
		break;

		// Display management.
		case Write(0x08e): {	// DIWSTRT
			const uint16_t value = cycle.value16();
			display_window_start_[0] = value & 0xff;
			display_window_start_[1] = value >> 8;
		} break;
		case Write(0x090): {	// DIWSTOP
			const uint16_t value = cycle.value16();
			display_window_stop_[0] = 0x100 | (value & 0xff);
			display_window_stop_[1] = value >> 8;
			display_window_stop_[1] |= ((value >> 7) & 0x100) ^ 0x100;
		} break;
		case Write(0x092):		// DDFSTRT
			if(fetch_window_[0] != cycle.value16()) {
				LOG("Fetch window start set to " << std::dec << cycle.value16());
			}
			fetch_window_[0] = cycle.value16();
		break;
		case Write(0x094):		// DDFSTOP
			// TODO: something in my interpretation of ddfstart and ddfstop
			// means a + 8 is needed below for high-res displays. Investigate.
			if(fetch_window_[1] != cycle.value16()) {
				LOG("Fetch window stop set to " << std::dec << fetch_window_[1]);
			}
			fetch_window_[1] = cycle.value16();
		break;

		// Bitplanes.
		case Write(0x0e0):	bitplanes_.set_pointer<0, 16>(cycle.value16());	break;	// BPL1PTH
		case Write(0x0e2):	bitplanes_.set_pointer<0, 0>(cycle.value16());	break;	// BPL1PTL
		case Write(0x0e4):	bitplanes_.set_pointer<1, 16>(cycle.value16());	break;	// BPL2PTH
		case Write(0x0e6):	bitplanes_.set_pointer<1, 0>(cycle.value16());	break;	// BPL2PTL
		case Write(0x0e8):	bitplanes_.set_pointer<2, 16>(cycle.value16());	break;	// BPL3PTH
		case Write(0x0ea):	bitplanes_.set_pointer<2, 0>(cycle.value16());	break;	// BPL3PTL
		case Write(0x0ec):	bitplanes_.set_pointer<3, 16>(cycle.value16());	break;	// BPL4PTH
		case Write(0x0ee):	bitplanes_.set_pointer<3, 0>(cycle.value16());	break;	// BPL4PTL
		case Write(0x0f0):	bitplanes_.set_pointer<4, 16>(cycle.value16());	break;	// BPL5PTH
		case Write(0x0f2):	bitplanes_.set_pointer<4, 0>(cycle.value16());	break;	// BPL5PTL
		case Write(0x0f4):	bitplanes_.set_pointer<5, 16>(cycle.value16());	break;	// BPL6PTH
		case Write(0x0f6):	bitplanes_.set_pointer<5, 0>(cycle.value16());	break;	// BPL6PTL

		case Write(0x100): {	// BPLCON0
			const auto value = cycle.value16();
			bitplanes_.set_control(value);
			is_high_res_ = value & 0x8000;
			hold_and_modify_ = value & 0x0800;
			dual_playfields_ = value & 0x0400;
			interlace_ = value & 0x0004;

//			LOG("New video control at " << std::dec << y_ << "; high res: " << is_high_res_ << " HAM: " << hold_and_modify_ << " dual: " << dual_playfields_ << " interlace: " << interlace_);
		} break;
		case Write(0x102): {	// BPLCON1
			const uint8_t delay = cycle.value8_low();
			odd_delay_ = delay & 0x0f;
			even_delay_ = delay >> 4;
		} break;
		case Write(0x104): {	// BPLCON2
			const auto value = cycle.value16();
			odd_priority_ = value & 7;
			even_priority_ = (value >> 3) & 7;
			even_over_odd_ = value & 0x40;
		} break;

		case Write(0x106):		// BPLCON3 (ECS)
			LOG("TODO: Bitplane control; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case Write(0x108):	bitplanes_.set_modulo<0>(cycle.value16());	break;	// BPL1MOD
		case Write(0x10a):	bitplanes_.set_modulo<1>(cycle.value16());	break;	// BPL2MOD

		case Write(0x110):
		case Write(0x112):
		case Write(0x114):
		case Write(0x116):
		case Write(0x118):
		case Write(0x11a):
			LOG("TODO: Bitplane data; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case Read(0x110):	case Read(0x112):	case Read(0x114):	case Read(0x116):
		case Read(0x118):	case Read(0x11a):
			cycle.set_value16(0xffff);
			LOG("Invalid read at " << PADHEX(6) << *cycle.address);
		break;

		// Blitter.
		case Read(0x040):	blitter_.set_control(0, 0xffff);				break;	// UGH. Have fallen into quite a hole here with my
		case Read(0x042):	blitter_.set_control(1, 0xffff);				break;	// Read/Write macros. TODO: some sort of canonical decode?
																					// Templatey to hit the usual Read/Write cases first?
		case Write(0x040):	blitter_.set_control(0, cycle.value16());		break;
		case Write(0x042):	blitter_.set_control(1, cycle.value16());		break;
		case Write(0x044):	blitter_.set_first_word_mask(cycle.value16());	break;
		case Write(0x046):	blitter_.set_last_word_mask(cycle.value16());	break;

		case Write(0x048):	blitter_.set_pointer<2, 16>(cycle.value16());	break;
		case Write(0x04a):	blitter_.set_pointer<2, 0>(cycle.value16());	break;
		case Write(0x04c):	blitter_.set_pointer<1, 16>(cycle.value16());	break;
		case Write(0x04e):	blitter_.set_pointer<1, 0>(cycle.value16());	break;
		case Write(0x050):	blitter_.set_pointer<0, 16>(cycle.value16());	break;
		case Write(0x052):	blitter_.set_pointer<0, 0>(cycle.value16());	break;
		case Write(0x054):	blitter_.set_pointer<3, 16>(cycle.value16());	break;
		case Write(0x056):	blitter_.set_pointer<3, 0>(cycle.value16());	break;

		case Write(0x058):	blitter_.set_size(cycle.value16());				break;
		case Write(0x05a):	blitter_.set_minterms(cycle.value16());			break;
//		case Write(0x05c):	blitter_.set_vertical_size(cycle.value16());	break;
//		case Write(0x05e):	blitter_.set_horizontal_size(cycle.value16());	break;

		case Write(0x060):	blitter_.set_modulo<2>(cycle.value16());		break;
		case Write(0x062):	blitter_.set_modulo<1>(cycle.value16());		break;
		case Write(0x064):	blitter_.set_modulo<0>(cycle.value16());		break;
		case Write(0x066):	blitter_.set_modulo<3>(cycle.value16());		break;

		case Write(0x070):	blitter_.set_data(2, cycle.value16());			break;
		case Write(0x072):	blitter_.set_data(1, cycle.value16());			break;
		case Write(0x074):	blitter_.set_data(0, cycle.value16());			break;

		// Audio.
#define Audio(index, pointer)	\
		case Write(pointer + 0):	audio_.set_pointer<index, 16>(cycle.value16());	break;	\
		case Write(pointer + 2):	audio_.set_pointer<index, 0>(cycle.value16());	break;	\
		case Write(pointer + 4):	audio_.set_length(index, cycle.value16());		break;	\
		case Write(pointer + 6):	audio_.set_period(index, cycle.value16());		break;	\
		case Write(pointer + 8):	audio_.set_volume(index, cycle.value16());		break;	\
		case Write(pointer + 10):	audio_.set_data(index, cycle.value16());		break;	\

		Audio(0, 0x0a0);
		Audio(1, 0x0b0);
		Audio(2, 0x0c0);
		Audio(3, 0x0d0);

#undef Audio

		// Copper.
		case Write(0x02e):	copper_.set_control(cycle.value16());			break;	// COPCON
		case Write(0x080):	copper_.set_pointer<0, 16>(cycle.value16());	break;	// COP1LCH
		case Write(0x082):	copper_.set_pointer<0, 0>(cycle.value16());		break;	// COP1LCL
		case Write(0x084):	copper_.set_pointer<1, 16>(cycle.value16());	break;	// COP2LCH
		case Write(0x086):	copper_.set_pointer<1, 0>(cycle.value16());		break;	// COP2LCL
		case Write(0x088):	case Read(0x088):
			copper_.reload<0>();
		break;
		case Write(0x08a):	case Read(0x08a):
			copper_.reload<1>();
		break;
		case Write(0x08c):
			LOG("TODO: coprocessor instruction fetch identity " << PADHEX(4) << cycle.value16());
		break;

		// Sprites.
#define Sprite(index, pointer, position)	\
		case Write(pointer + 0):	sprites_[index].set_pointer<0, 16>(cycle.value16());	break;	\
		case Write(pointer + 2):	sprites_[index].set_pointer<0, 0>(cycle.value16());		break;	\
		case Write(position + 0):	sprites_[index].set_start_position(cycle.value16());	break;	\
		case Write(position + 2):	sprites_[index].set_stop_and_control(cycle.value16());	break;	\
		case Write(position + 4):	sprites_[index].set_image_data(0, cycle.value16());		break;	\
		case Write(position + 6):	sprites_[index].set_image_data(1, cycle.value16());		break;

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
		case Write(0x180):	case Write(0x182):	case Write(0x184):	case Write(0x186):
		case Write(0x188):	case Write(0x18a):	case Write(0x18c):	case Write(0x18e):
		case Write(0x190):	case Write(0x192):	case Write(0x194):	case Write(0x196):
		case Write(0x198):	case Write(0x19a):	case Write(0x19c):	case Write(0x19e):
		case Write(0x1a0):	case Write(0x1a2):	case Write(0x1a4):	case Write(0x1a6):
		case Write(0x1a8):	case Write(0x1aa):	case Write(0x1ac):	case Write(0x1ae):
		case Write(0x1b0):	case Write(0x1b2):	case Write(0x1b4):	case Write(0x1b6):
		case Write(0x1b8):	case Write(0x1ba):	case Write(0x1bc):	case Write(0x1be): {
			// Store once in regular, linear order.
			const auto entry_address = (register_address - 0x180) >> 1;
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[entry_address]);
			entry[0] = cycle.value8_high();
			entry[1] = cycle.value8_low();

			// Also store in bit-swizzled order. In this array,
			// instead of being indexed as [b4 b3 b2 b1 b0], index
			// as [b3 b1 b4 b2 b0], and include a second set of the
			// 32 colours, stored as half-bright.
			const auto swizzled_address =
				(entry_address&0x01) |
				((entry_address&0x02) << 2) |
				((entry_address&0x04) >> 1) |
				((entry_address&0x08) << 1) |
				((entry_address&0x10) >> 2);
			uint8_t *const swizzled_entry = reinterpret_cast<uint8_t *>(&swizzled_palette_[swizzled_address]);
			swizzled_entry[0] = cycle.value8_high();
			swizzled_entry[1] = cycle.value8_low();

			swizzled_entry[64] = (swizzled_entry[0] >> 1) & 0x77;
			swizzled_entry[65] = (swizzled_entry[1] >> 1) & 0x77;
		} break;
	}

#undef ApplySetClear

#undef Write
#undef Read
#undef RW
}

// MARK: - Bitplanes.

bool Chipset::Bitplanes::advance_dma(int cycle) {
#define BIND_CYCLE(offset, plane)								\
	case offset:												\
		if(plane_count_ > plane) {								\
			next[plane] = ram_[pointer_[plane] & ram_mask_];	\
			++pointer_[plane];									\
			if constexpr (!plane) {								\
				chipset_.post_bitplanes(next);					\
			}													\
			return true;										\
		}														\
	return false;

	if(is_high_res_) {
		switch(cycle&3) {
			default: return false;
			BIND_CYCLE(0, 3);
			BIND_CYCLE(1, 1);
			BIND_CYCLE(2, 2);
			BIND_CYCLE(3, 0);
		}
	} else {
		switch(cycle&7) {
			default: return false;
			/* Omitted: 0. */
			BIND_CYCLE(1, 3);
			BIND_CYCLE(2, 5);
			BIND_CYCLE(3, 1);
			/* Omitted: 4. */
			BIND_CYCLE(5, 2);
			BIND_CYCLE(6, 4);
			BIND_CYCLE(7, 0);
		}
	}

	return false;

#undef BIND_CYCLE
}

void Chipset::Bitplanes::do_end_of_line() {
	// Apply modulos here. Posssibly correct?
	pointer_[0] += modulos_[1];
	pointer_[2] += modulos_[1];
	pointer_[4] += modulos_[1];

	pointer_[1] += modulos_[0];
	pointer_[3] += modulos_[0];
	pointer_[5] += modulos_[0];
}

void Chipset::Bitplanes::set_control(uint16_t control) {
	is_high_res_ = control & 0x8000;
	plane_count_ = (control >> 12) & 7;

	// TODO: who really has responsibility for clearing the other
	// bit plane fields?
	std::fill(next.begin() + plane_count_, next.end(), 0);
	if(plane_count_ == 7) {
		plane_count_ = 4;
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

// MARK: - Mouse.

int Chipset::Mouse::get_number_of_buttons() {
	return 2;
}

void Chipset::Mouse::set_button_pressed(int button, bool is_set) {
	switch(button) {
		case 0:
			cia_state_ = (cia_state_ &~ 0x40) | (is_set ? 0 : 0x40);
		break;
		default:
		break;
	}
}

uint8_t Chipset::Mouse::get_cia_button() const {
	return cia_state_;
}

void Chipset::Mouse::reset_all_buttons() {
	cia_state_ = 0xff;
}

void Chipset::Mouse::move(int x, int y) {
	position_[0] += x;
	position_[1] += y;
}

Inputs::Mouse &Chipset::get_mouse() {
	return mouse_;
}

uint16_t Chipset::Mouse::get_position() {
	// The Amiga hardware retains only eight bits of position
	// for the mouse; its software polls frequently and maps
	// changes into a larger space.
	//
	// On modern computers with 5k+ displays and trackpads, it
	// proved empirically possible to overflow the hardware
	// counters more quickly than software would poll.
	//
	// Therefore the approach taken for mapping mouse motion
	// into the Amiga is to do it in steps of no greater than
	// [-128, +127], as per the below.
	const int pending[] = {
		position_[0], position_[1]
	};

	const int8_t change[] = {
		int8_t(std::clamp(pending[0], -128, 127)),
		int8_t(std::clamp(pending[1], -128, 127))
	};

	position_[0] -= change[0];
	position_[1] -= change[1];
	declared_position_[0] += change[0];
	declared_position_[1] += change[1];

	return uint16_t(
		(declared_position_[1] << 8) |
		declared_position_[0]
	);
}

// MARK: - Joystick.

// TODO: add second fire button.

Chipset::Joystick::Joystick() :
	ConcreteJoystick({
						Input(Input::Up),
						Input(Input::Down),
						Input(Input::Left),
						Input(Input::Right),
						Input(Input::Fire, 0),
					}) {}

void Chipset::Joystick::did_set_input(const Input &input, bool is_active) {
	// Accumulate state.
	inputs_[input.type] = is_active;

	// Determine what that does to the two position bits.
	const auto low =
		(inputs_[Input::Type::Down] ^ inputs_[Input::Type::Right]) |
		(inputs_[Input::Type::Right] << 1);
	const auto high =
		(inputs_[Input::Type::Up] ^ inputs_[Input::Type::Left]) |
		(inputs_[Input::Type::Left] << 1);

	// Ripple upwards if that affects the mouse position counters.
	const uint8_t previous_low = position_ & 3;
	uint8_t low_upper = (position_ >> 2) & 0x3f;
	const uint8_t previous_high = (position_ >> 8) & 3;
	uint8_t high_upper = (position_ >> 10) & 0x3f;

	if(!low && previous_low == 3) ++low_upper;
	if(!previous_low && low == 3) --low_upper;
	if(!high && previous_high == 3) ++high_upper;
	if(!previous_high && high == 3) --high_upper;

	position_ = uint16_t(
		low | ((low_upper & 0x3f) << 2) |
		(high << 8) | ((high_upper & 0x3f) << 10)
	);
}

uint16_t Chipset::Joystick::get_position() const {
	return position_;
}

uint8_t Chipset::Joystick::get_cia_button() const {
	return inputs_[Input::Type::Fire] ? 0xbf : 0xff;
}

// MARK: - Synchronisation.

void Chipset::flush() {
}
