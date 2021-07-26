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

#include <cassert>

using namespace Amiga;

namespace {

enum InterruptFlag: uint16_t {
	SerialPortTransmit		= 1 << 0,
	DiskBlock				= 1 << 1,
	Software				= 1 << 2,
	IOPortsAndTimers		= 1 << 3,
	Copper					= 1 << 4,
	VerticalBlank			= 1 << 5,
	Blitter					= 1 << 6,
	AudioChannel0			= 1 << 7,
	AudioChannel1			= 1 << 8,
	AudioChannel2			= 1 << 9,
	AudioChannel3			= 1 << 10,
	SerialPortReceive		= 1 << 11,
	DiskSyncMatch			= 1 << 12,
	External				= 1 << 13,
};

}

Chipset::Chipset(uint16_t *ram, size_t size) :
	blitter_(ram, size),
	crt_(908, 4, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4) {
}

Chipset::Changes Chipset::run_for(HalfCycles length) {
	Changes changes;

	// Update raster position, spooling out something that isn't yet actual graphics.
	auto pixels_remaining = length.as<int>();
	while(pixels_remaining) {
		// Determine number of pixels left on this line.
		int line_pixels = std::min(pixels_remaining, line_length_ - x_);
		pixels_remaining -= line_pixels;

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

#define LINK(location, action, length) \
	if(x_ < (location) && final_x >= (location))	{	\
		crt_.action((length) * 4);	\
	}

		const int final_x = x_ + line_pixels;
		if(y_ < vertical_blank_height_) {
			// Put three lines of sync at the centre of the vertical blank period.
			// TODO: offset by half a line if interlaced.

			const int midline = vertical_blank_height_ >> 1;
			if(y_ < midline - 1 || y_ > midline + 1) {
				LINK(blank1, output_blank, blank1);
				LINK(sync, output_sync, sync - blank1);
				LINK(line_length_, output_blank, line_length_ - sync);
			} else {
				LINK(blank1, output_sync, blank1);
				LINK(sync, output_blank, sync - blank1);
				LINK(line_length_, output_sync, line_length_ - sync);
			}
		} else {
			// Output the correct sequence of blanks, syncs and burst atomically.
			LINK(blank1, output_blank, blank1);
			LINK(sync, output_sync, sync - blank1);
			LINK(blank2, output_blank, blank2 - sync);
			LINK(burst, output_default_colour_burst, burst - blank2);	// TODO: only if colour enabled.
			LINK(blank3, output_blank, blank3 - burst);

			// Output generic white to fill the rest of the line.
			if(final_x > blank3) {
				const int start_x = std::max(blank3, x_);

				uint16_t *const pixels = reinterpret_cast<uint16_t *>(crt_.begin_data(2));
				if(pixels) {
					*pixels = 0xffff;
				}
				crt_.output_data((final_x - start_x) * 4, 1);
			}
		}

		// Advance intraline counter and possibly ripple upwards into
		// lines and fields.
		x_ += line_pixels;
		if(x_ == line_length_) {
			++changes.hsyncs;

			x_ = 0;
			++y_;

			if(y_ == frame_height_) {
				++changes.vsyncs;
				interrupt_requests_ |= InterruptFlag::VerticalBlank;
				update_interrupts();

				y_ = 0;
			}
		}
	}
#undef LINK

	changes.interrupt_level = interrupt_level_;
	return changes;
}

void Chipset::update_interrupts() {
	interrupt_level_ = 0;

	const uint16_t enabled_requests = interrupt_enable_ & interrupt_requests_ & 0x3fff;
	if(enabled_requests && (interrupt_enable_ & 0x4000)) {
		if(enabled_requests & (InterruptFlag::External)) {
			interrupt_level_ = 6;
		} else if(enabled_requests & (InterruptFlag::SerialPortReceive | InterruptFlag::DiskSyncMatch)) {
			interrupt_level_ = 5;
		} else if(enabled_requests & (InterruptFlag::AudioChannel0 | InterruptFlag::AudioChannel1 | InterruptFlag::AudioChannel2 | InterruptFlag::AudioChannel3)) {
			interrupt_level_ = 4;
		} else if(enabled_requests & (InterruptFlag::Copper | InterruptFlag::VerticalBlank | InterruptFlag::Blitter)) {
			interrupt_level_ = 3;
		} else if(enabled_requests & (InterruptFlag::External)) {
			interrupt_level_ = 2;
		} else if(enabled_requests & (InterruptFlag::SerialPortTransmit | InterruptFlag::DiskBlock | InterruptFlag::Software)) {
			interrupt_level_ = 1;
		}
	}
}

void Chipset::perform(const CPU::MC68000::Microcycle &cycle) {
	using Microcycle = CPU::MC68000::Microcycle;

#define RW(address)		(address & 0xffe) | ((cycle.operation & Microcycle::Read) << 12)
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

	switch(RW(*cycle.address)) {
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
			const uint16_t position = uint16_t((x_ << 8) | (y_ & 0xff));
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
		case Write(0x020):	case Write(0x022):	case Write(0x024):
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
		break;

		case Write(0x09c):
			ApplySetClear(interrupt_requests_);
			update_interrupts();
			LOG("Interrupt request modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{interrupt_requests_});
		break;
		case Read(0x01e):
			cycle.set_value16(interrupt_requests_);
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
		case Write(0x100):
		case Write(0x102):
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

		case Write(0x048):	blitter_.set_pointer(2, 16, cycle.value16());	break;
		case Write(0x04a):	blitter_.set_pointer(2, 0, cycle.value16());	break;
		case Write(0x04c):	blitter_.set_pointer(1, 16, cycle.value16());	break;
		case Write(0x04e):	blitter_.set_pointer(1, 0, cycle.value16());	break;
		case Write(0x050):	blitter_.set_pointer(0, 16, cycle.value16());	break;
		case Write(0x052):	blitter_.set_pointer(0, 0, cycle.value16());	break;
		case Write(0x054):	blitter_.set_pointer(3, 16, cycle.value16());	break;
		case Write(0x056):	blitter_.set_pointer(3, 0, cycle.value16());	break;

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
			LOG("TODO: coprocessor control " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x080):
			LOG("TODO: coprocessor first location register high " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x082):
			LOG("TODO: coprocessor first location register low " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x084):
			LOG("TODO: coprocessor second location register high " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x086):
			LOG("TODO: coprocessor second location register low " << PADHEX(4) << cycle.value16());
		break;
		case Write(0x088):	case Read(0x088):
			LOG("TODO: coprocessor restart at first location");
		break;
		case Write(0x08a):	case Read(0x08a):
			LOG("TODO: coprocessor restart at second location");
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
		case Write(0x1b8):	case Write(0x1ba):	case Write(0x1bc):	case Write(0x1be):
			LOG("TODO: colour palette; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
		break;
	}

#undef ApplySetClear

#undef Write
#undef Read
#undef RW
}

// MARK: - Sprites.

void Chipset::Sprite::set_pointer(int shift, uint16_t value) {
	LOG("Sprite pointer with shift " << shift << " to " << PADHEX(4) << value);
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
