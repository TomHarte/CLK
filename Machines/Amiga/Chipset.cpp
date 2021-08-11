//
//  Chipset.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Chipset.hpp"

//#define NDEBUG
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

}

Chipset::Chipset(uint16_t *ram, size_t size) :
	blitter_(ram, size),
	bitplanes_(*this, ram, size),
	copper_(*this, ram, size),
	disk_(*this, ram, size),
	crt_(908, 4, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4) {
}

Chipset::Changes Chipset::run_for(HalfCycles length) {
	return run<false>(length);
}

Chipset::Changes Chipset::run_until_cpu_slot() {
	return run<true>();
}

void Chipset::set_cia_interrupts(bool cia_a, bool cia_b) {
	// TODO: are these really latched, or are they active live?
	interrupt_requests_ &= ~InterruptMask<InterruptFlag::IOPortsAndTimers, InterruptFlag::External>::value;
	interrupt_requests_ |=
		(cia_a ? InterruptMask<InterruptFlag::IOPortsAndTimers>::value : 0) |
		(cia_b ? InterruptMask<InterruptFlag::External>::value : 0);
	update_interrupts();
}

void Chipset::posit_interrupt(InterruptFlag flag) {
	interrupt_requests_ |= uint16_t(flag);
	update_interrupts();
}

bool Chipset::Copper::advance(uint16_t position) {
	switch(state_) {
		default: return false;

		case State::Waiting:
			// TODO: blitter-finished bit.
			if((position & position_mask_) >= instruction_[0]) {
				state_ = State::FetchFirstWord;
			}
		return false;

		case State::FetchFirstWord:
			instruction_[0] = ram_[address_ & ram_mask_];
			++address_;
			state_ = State::FetchSecondWord;
		break;

		case State::FetchSecondWord: {
			const bool should_skip_move = skip_next_;
			skip_next_ = false;

			instruction_[1] = ram_[address_ & ram_mask_];
			++address_;

			if(!(instruction_[0] & 1)) {
				// A MOVE.

				if(!should_skip_move) {
					// Stop if this move would be a privilege violation.
					instruction_[0] &= 0x1fe;
					if((instruction_[0] < 0x10) || (instruction_[0] < 0x20 && !(control_&1))) {
						LOG("Invalid Copper MOVE to " << PADHEX(4) << instruction_[0] << "; stopping");
						state_ = State::Stopped;
						break;
					}

					// Construct a 68000-esque Microcycle in order to be able to perform the access.
					CPU::MC68000::Microcycle cycle;
					cycle.operation = CPU::MC68000::Microcycle::SelectWord;
					uint32_t full_address = instruction_[0];
					CPU::RegisterPair16 data = instruction_[1];
					cycle.address = &full_address;
					cycle.value = &data;
					chipset_.perform(cycle);
				}

				// Roll onto the next command.
				state_ = State::FetchFirstWord;
				break;
			}

			// Prepare for a position comparison.
			position_mask_ = 0x8001 | (instruction_[1] & 0x7ffe);
			instruction_[0] &= position_mask_;

			if(!(instruction_[1] & 1)) {
				// A WAIT. Just note that this is now waiting; the proper test
				// will be applied from the next potential `advance` onwards.
				state_ = State::Waiting;
				break;
			}

			// Neither a WAIT nor a MOVE => a SKIP.

			// TODO: blitter-finished bit.
			skip_next_ = (position & position_mask_) >= instruction_[0];
			state_ = State::FetchFirstWord;
		} break;
	}

	return true;
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

	// A complete from-thin-air guess:
	//
	//	7 cycles blank;
	//	17 cycles sync;
	//	3 cycles blank;
	//	9 cycles colour burst;
	//	7 cycles blank.
	constexpr int blank1	= 7;
	constexpr int sync		= 17 + blank1;
	constexpr int blank2 	= 3 + sync;
	constexpr int burst 	= 9 + blank2;
	constexpr int blank3 	= 7 + burst;
	static_assert(blank3 == 43);

#define LINK(location, action, length)	\
	if(cycle == (location))	{			\
		crt_.action((length) * 4);		\
	}

	if(y_ < vertical_blank_height_) {
		// Put three lines of sync at the centre of the vertical blank period.
		// Offset by half a line if interlaced and on an odd frame.

		const int midline = vertical_blank_height_ >> 1;
		if(frame_height_ & 1) {
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
		// Output the correct sequence of blanks, syncs and burst atomically.
		LINK(blank1, output_blank, blank1);
		LINK(sync, output_sync, sync - blank1);
		LINK(blank2, output_blank, blank2 - sync);
		LINK(burst, output_default_colour_burst, burst - blank2);	// TODO: only if colour enabled.
		LINK(blank3, output_blank, blank3 - burst);

		// Output colour 0 to fill the rest of the line; Kickstart uses this
		// colour to post the error code. TODO: actual pixels, etc.
		if(cycle == line_length_ - 1) {
			uint16_t *const pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
			if(pixels) {
				*pixels = palette_[0];
			}
			crt_.output_data((cycle - blank3) * 4, 1);
		}
	}

#undef LINK
}

template <int cycle, bool stop_if_cpu> bool Chipset::perform_cycle() {
	constexpr auto BlitterFlag	= DMAMask<DMAFlag::Blitter, DMAFlag::AllBelow>::value;
	constexpr auto BitplaneFlag	= DMAMask<DMAFlag::Bitplane, DMAFlag::AllBelow>::value;
	constexpr auto CopperFlag	= DMAMask<DMAFlag::Copper, DMAFlag::AllBelow>::value;
	constexpr auto DiskFlag		= DMAMask<DMAFlag::Disk, DMAFlag::AllBelow>::value;

	// Update state as to whether bitplane fetching should happen now.
	//
	// TODO: figure out how the hard stops factor into this.
	fetch_horizontal_ |= cycle == display_window_start_[0];
	fetch_horizontal_ &= cycle != display_window_stop_[0];

	// Top priority: bitplane collection.
	if((dma_control_ & BitplaneFlag) == BitplaneFlag) {
		// TODO: offer a cycle for bitplane collection.
		// Probably need to indicate odd or even?
		if(fetch_horizontal_ && fetch_vertical_ && bitplanes_.template advance<cycle&1>()) {
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
		if constexpr (cycle >= 4 && cycle <= 6) {
			if((dma_control_ & DiskFlag) == DiskFlag) {
				if(disk_.advance()) {
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
			if(copper_.advance(uint16_t(((y_ & 0xff) << 8) | (cycle & 0xfe)))) {
				return false;
			}
		} else {
			copper_.stop();
		}
	}

	// Down here: give first refusal to the Blitter, otherwise
	// pass on to the CPU.
	return (dma_control_ & BlitterFlag) != BlitterFlag || !blitter_.advance();
}

template <bool stop_on_cpu> int Chipset::advance_slots(int first_slot, int last_slot) {
	if(first_slot == last_slot) {
		return -1;
	}

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

		// Advance intraline counter and pcoossibly ripple upwards into
		// lines and fields.
		if(line_cycle_ == (line_length_ * 4)) {
			++changes.hsyncs;

			line_cycle_ = 0;
			++y_;

			fetch_vertical_ |= y_ == display_window_start_[1];
			fetch_vertical_ &= y_ != display_window_stop_[1];

			if(did_fetch_) {
				// TODO: find out when modulos are actually applied, since
				// they're dynamically programmable.
				bitplanes_.do_end_of_line();
				did_fetch_ = false;
			}

			if(y_ == frame_height_) {
				++changes.vsyncs;
				interrupt_requests_ |= InterruptMask<InterruptFlag::VerticalBlank>::value;
				update_interrupts();

				y_ = 0;

				// TODO: the manual is vague on when this happens. Try to find out.
				copper_.reload<0>();
			}
		}
		assert(line_cycle_ < line_length_ * 4);
	}

	changes.interrupt_level = interrupt_level_;
	return changes;
}

void Chipset::post_bitplanes(const BitplaneData &data) {
	// Convert to future pixels.
	const int odd_offset = line_cycle_ + odd_delay_;
	const int even_offset = line_cycle_ + odd_delay_;
	for(int x = 0; x < 16; x++) {
		const uint16_t mask = uint16_t(1 << x);
		even_playfield_[x + even_offset] = uint8_t(
			((data[0] & mask) | ((data[2] & mask) << 1) | ((data[4] & mask) << 2)) >> x
		);
		odd_playfield_[x + odd_offset] = uint8_t(
			((data[1] & mask) | ((data[3] & mask) << 1) | ((data[5] & mask) << 2)) >> x
		);
	}
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

#define ApplySetClear(target)	{			\
	const uint16_t value = cycle.value16();	\
	if(value & 0x8000) {					\
		target |= (value & 0x7fff);			\
	} else {								\
		target &= ~(value & 0x7fff);		\
	}										\
}

	const uint32_t register_address = *cycle.address & 0x1fe;
	switch(RW(register_address)) {
		default:
			LOG("Unimplemented chipset " << (cycle.operation & Microcycle::Read ? "read" : "write") <<  " " << PADHEX(6) << *cycle.address);
			assert(false);
		break;

		// Raster position.
		case Read(0x004): {
			const uint16_t position = uint16_t(y_ >> 8);
			LOG("Read vertical position high " << PADHEX(4) << position);
			cycle.set_value16(position);
		} break;
		case Read(0x006): {
//			const uint16_t position = uint16_t(((line_cycle_ << 6) & 0xff00) | (y_ & 0x00ff));
			const uint16_t position = 0xd1ef;	// TODO: !!!
			LOG("Read position low " << PADHEX(4) << position);
			cycle.set_value16(position);
		} break;

		case Write(0x02a):
			LOG("TODO: write vertical position high " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x02c):
			LOG("TODO: write vertical position low " << PADHEX(4) << cycle.value16());
		break;

		// Joystick/mouse input.
		case Read(0x00a):
		case Read(0x00c):
			LOG("TODO: Joystick/mouse position " << PADHEX(4) << *cycle.address);
			cycle.set_value16(0x8080);
		break;

		case Write(0x034):
			LOG("TODO: pot port start");
		break;
		case Read(0x016):
			LOG("TODO: pot port read");
			cycle.set_value16(0xff00);
		break;

		// Disk DMA.
		case Write(0x020):	disk_.set_pointer<0, 16>(cycle.value16());	break;
		case Write(0x022):	disk_.set_pointer<0, 0>(cycle.value16());	break;
		case Write(0x024):	disk_.set_length(cycle.value16());			break;

		case Write(0x026):
			LOG("TODO: disk DMA; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		// Refresh.
		case Write(0x028):
			LOG("TODO (maybe): refresh; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		// Serial port.
		case Write(0x030):
			LOG("TODO: serial data: " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x032):
			LOG("TODO: serial control: " << PADHEX(4) << cycle.value16());
			serial_.set_control(cycle.value16());
		break;

		// DMA management.
		case Read(0x002):
			LOG("DMA control and status read");
			cycle.set_value16(dma_control_ | blitter_.get_status());
		break;
		case Write(0x096):
			ApplySetClear(dma_control_);
			LOG("DMA control modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{dma_control_});
		break;

		// Interrupts.
		case Write(0x09a):
			ApplySetClear(interrupt_enable_);
			update_interrupts();
			LOG("Interrupt enable mask modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{interrupt_enable_});
		break;
		case Read(0x01c):
			cycle.set_value16(interrupt_enable_);
			LOG("Interrupt enable mask read: " << PADHEX(4) << interrupt_enable_);
		break;

		case Write(0x09c):
			ApplySetClear(interrupt_requests_);
			update_interrupts();
			LOG("Interrupt request modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{interrupt_requests_});
		break;
		case Read(0x01e):
			cycle.set_value16(interrupt_requests_);
			LOG("Interrupt requests read: " << PADHEX(4) << interrupt_requests_);
		break;

		// Display management.
		case Write(0x08e): {
			const uint16_t value = cycle.value16();
			display_window_start_[0] = value & 0xff;
			display_window_start_[1] = value >> 8;
			LOG("Display window start set to " << std::dec << display_window_start_[0] << ", " << display_window_start_[1]);
		} break;
		case Write(0x090): {
			const uint16_t value = cycle.value16();
			display_window_stop_[0] = 0x100 | (value & 0xff);
			display_window_stop_[1] = value >> 8;
			display_window_stop_[1] |= ((value >> 7) & 0x100) ^ 0x100;
			LOG("Display window stop set to " << std::dec << display_window_stop_[0] << ", " << display_window_stop_[1]);
		} break;
		case Write(0x092):
			fetch_window_[0] = uint16_t((cycle.value16() & 0xfc) << 1);
			LOG("Fetch window start set to " << std::dec << fetch_window_[0]);
		break;
		case Write(0x094):
			fetch_window_[1] = uint16_t((cycle.value16() & 0xfc) << 1);
			LOG("Fetch window stop set to " << std::dec << fetch_window_[1]);
		break;

		// Bitplanes.
		case Write(0x0e0):	bitplanes_.set_pointer<0, 16>(cycle.value16());	break;
		case Write(0x0e2):	bitplanes_.set_pointer<0, 0>(cycle.value16());	break;
		case Write(0x0e4):	bitplanes_.set_pointer<1, 16>(cycle.value16());	break;
		case Write(0x0e6):	bitplanes_.set_pointer<1, 0>(cycle.value16());	break;
		case Write(0x0e8):	bitplanes_.set_pointer<2, 16>(cycle.value16());	break;
		case Write(0x0ea):	bitplanes_.set_pointer<2, 0>(cycle.value16());	break;
		case Write(0x0ec):	bitplanes_.set_pointer<3, 16>(cycle.value16());	break;
		case Write(0x0ee):	bitplanes_.set_pointer<3, 0>(cycle.value16());	break;
		case Write(0x0f0):	bitplanes_.set_pointer<4, 16>(cycle.value16());	break;
		case Write(0x0f2):	bitplanes_.set_pointer<4, 0>(cycle.value16());	break;
		case Write(0x0f4):	bitplanes_.set_pointer<5, 16>(cycle.value16());	break;
		case Write(0x0f6):	bitplanes_.set_pointer<5, 0>(cycle.value16());	break;

		case Write(0x102): {
			const uint8_t delay = cycle.value8_low();
			odd_delay_ = delay & 0x0f;
			even_delay_ = delay >> 4;
		} break;

		case Write(0x100):
			bitplanes_.set_control(cycle.value16());
		break;

		case Write(0x104):
		case Write(0x106):
			LOG("TODO: Bitplane control; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

		case Write(0x108):
		case Write(0x10a):
			LOG("TODO: Bitplane modulo; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;

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
		case Write(0x05c):	blitter_.set_vertical_size(cycle.value16());	break;
		case Write(0x05e):	blitter_.set_horizontal_size(cycle.value16());	break;

		case Write(0x060):	blitter_.set_modulo(2, cycle.value16());		break;
		case Write(0x062):	blitter_.set_modulo(1, cycle.value16());		break;
		case Write(0x064):	blitter_.set_modulo(0, cycle.value16());		break;
		case Write(0x066):	blitter_.set_modulo(3, cycle.value16());		break;

		case Write(0x070):	blitter_.set_data(2, cycle.value16());			break;
		case Write(0x072):	blitter_.set_data(1, cycle.value16());			break;
		case Write(0x074):	blitter_.set_data(0, cycle.value16());			break;

		// Paula.
		case Write(0x09e):
		case Write(0x0a0):	case Write(0x0a2):	case Write(0x0a4):	case Write(0x0a6):
		case Write(0x0a8):	case Write(0x0aa):
		case Write(0x0b0):	case Write(0x0b2):	case Write(0x0b4):	case Write(0x0b6):
		case Write(0x0b8):	case Write(0x0ba):
		case Write(0x0c0):	case Write(0x0c2):	case Write(0x0c4):	case Write(0x0c6):
		case Write(0x0c8):	case Write(0x0ca):
		case Write(0x0d0):	case Write(0x0d2):	case Write(0x0d4):	case Write(0x0d6):
		case Write(0x0d8):	case Write(0x0da):
			LOG("TODO: Paula write " << PADHEX(2) << (*cycle.address & 0xff) << " " << PADHEX(4) << cycle.value16());
		break;

		// Copper.
		case Write(0x02e):
			LOG("Coprocessor control " << PADHEX(4) << cycle.value16());
			copper_.set_control(cycle.value16());
		break;
		case Write(0x080):
			LOG("Coprocessor first location register high " << PADHEX(4) << cycle.value16());
			copper_.set_pointer<0, 16>(cycle.value16());
		break;
		case Write(0x082):
			LOG("Coprocessor first location register low " << PADHEX(4) << cycle.value16());
			copper_.set_pointer<0, 0>(cycle.value16());
		break;
		case Write(0x084):
			LOG("Coprocessor second location register high " << PADHEX(4) << cycle.value16());
			copper_.set_pointer<1, 16>(cycle.value16());
		break;
		case Write(0x086):
			LOG("Coprocessor second location register low " << PADHEX(4) << cycle.value16());
			copper_.set_pointer<1, 0>(cycle.value16());
		break;
		case Write(0x088):	case Read(0x088):
			LOG("Coprocessor restart at first location");
			copper_.reload<0>();
		break;
		case Write(0x08a):	case Read(0x08a):
			LOG("Coprocessor restart at second location");
			copper_.reload<1>();
		break;
		case Write(0x08c):
			LOG("TODO: coprocessor instruction fetch identity " << PADHEX(4) << cycle.value16());
		break;

		// Sprites.
#define Sprite(index, pointer, position)	\
		case Write(pointer + 0):	sprites_[index].set_pointer(16, cycle.value16());		break;	\
		case Write(pointer + 2):	sprites_[index].set_pointer(0, cycle.value16());		break;	\
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
			LOG("Colour palette; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);

			// Store once in regular, linear order.
			const auto entry_address = (register_address - 0x180) >> 1;
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[entry_address]);
			entry[0] = cycle.value8_high();
			entry[1] = cycle.value8_low();

			// Also store in bit-swizzled order. In this array,
			// instead of being indexed as [b4 b3 b2 b1 b0], index
			// as [b3 b1 b4 b2 b0]. This is related to the dual/single-playfield
			// decision being made relatively late in the planar -> chunky
			// conversion performed by this implementation.
			const auto swizzled_address =
				(entry_address&0x01) |
				((entry_address&0x02) << 2) |
				((entry_address&0x04) >> 1) |
				((entry_address&0x08) << 1) |
				((entry_address&0x10) >> 2);
			uint8_t *const swizzled_entry = reinterpret_cast<uint8_t *>(&swizzled_palette_[swizzled_address]);
			swizzled_entry[0] = cycle.value8_high();
			swizzled_entry[1] = cycle.value8_low();
		} break;
	}

#undef ApplySetClear

#undef Write
#undef Read
#undef RW
}

// MARK: - Bitplanes.

template <bool is_odd> bool Chipset::Bitplanes::advance() {
	// Offset is preincremented in order to simplify the exit
	// pathways below. As a result all in-switch offsets are
	// off by 1. This is dealt with in the macro.
	++collection_offset_;

	// TODO: possibly dispatch a BitplaneData.
	// The chipset has responsibility for applying a delay,
	// and the pixel-level start/stop boundaries.

#define BIND_CYCLE(offset, plane)	\
	case (offset + 1)&7:			\
		if(plane_count_ > plane) {	\
			next[plane] = ram_[pointer_[plane] & ram_mask_];	\
			++pointer_[plane];	\
			if constexpr (!plane) {			\
				chipset_.post_bitplanes(next);	\
			}						\
			return true;		\
		}	\
	return false;

	if(is_high_res_) {
		// TODO: I'm unclear whether this is correct, or merely
		// an artefact of the way the Hardware Reference Manual
		// depicts per-line DMA responsibilities.
		if(collection_offset_ < 4) {
			return false;
		}

		switch(collection_offset_&7) {
			default: return false;
			BIND_CYCLE(0, 3);
			BIND_CYCLE(1, 1);
			BIND_CYCLE(2, 2);
			BIND_CYCLE(3, 0);
			BIND_CYCLE(4, 3);
			BIND_CYCLE(5, 1);
			BIND_CYCLE(6, 2);
			BIND_CYCLE(7, 0);
		}
	} else {
		switch(collection_offset_&7) {
			default: return false;
			BIND_CYCLE(1, 3);
			BIND_CYCLE(2, 5);
			BIND_CYCLE(3, 1);
			BIND_CYCLE(5, 2);
			BIND_CYCLE(6, 4);
			BIND_CYCLE(7, 0);
		}
	}

	return false;
}

void Chipset::Bitplanes::do_end_of_line() {
	// TODO: apply modulos.
	collection_offset_ = 0;
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


// MARK: - Sprites.

void Chipset::Sprite::set_pointer(int shift, uint16_t value) {
	LOG("Sprite pointer with shift " << std::dec << shift << " to " << PADHEX(4) << value);
}

void Chipset::Sprite::set_start_position(uint16_t value) {
	LOG("Sprite start position " << PADHEX(4) << value);
}

void Chipset::Sprite::set_stop_and_control(uint16_t value) {
	LOG("Sprite stop and control " << PADHEX(4) << value);
}

void Chipset::Sprite::set_image_data(int slot, uint16_t value) {
	LOG("Sprite image data " << slot << " to " << PADHEX(4) << value);
}

// MARK: - Disk.

bool Chipset::DiskDMA::advance() {
	if(!dma_enable_) return false;

	if(!write_) {
		// TODO: run an actual PLL, collect actual disk data.
		if(length_) {
			ram_[pointer_[0] & ram_mask_] = 0xffff;
			++pointer_[0];
			--length_;

			if(!length_) {
				chipset_.posit_interrupt(InterruptFlag::DiskBlock);
			}

			return true;
		}
	}

	return false;
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

