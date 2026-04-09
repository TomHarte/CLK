//
//  MemoryMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <vector>

#include "Machines/Utility/MemoryFuzzer.hpp"
#include "Outputs/Log.hpp"

namespace Thomson::MO5 {

//
// Notes on the layout of the standard MO ROM images:
//
//	The MO5 images are 16kb in total, representing what should go into the
//	final 16kb of the memory map.
//
//	The MO6 images are 64kb in total:
//
//	* the first 12kb is the BASIC 1 portion;
//	* the next 12 kb is <something>, possibly the GUI?
//	* what is possibly a 4kb gap then follows — it's less than 300 bytes of content, then filler — though it ends in C000 which could be a vector?;
//	* there are then probably two meaningful pages of a full 16kb each (the first of which is definitely to do with BASIC 128, presumably the second also); and
//	* the final 4kb is the monitor, which resides permanently at $f000–;

template <bool is_mo6>
struct MemoryMap {
public:
	MemoryMap() {
		// Install monitor at 0xf000.
		set_read(0xf, monitor_.data());

		// Install RAM.
		page_video(false);
		page_ram();

		// Set RAM to an undefined state.
		Memory::Fuzz(ram_);
		Memory::Fuzz(video_);
	}

	// MARK: - ROM installation and connection for video.

	template <typename IteratorT>
	void set_monitor(const IteratorT begin, const IteratorT end) {
		assert(std::distance(begin, end) == 0x1000);
		std::copy(begin, end, monitor_.begin());
	}

	template <typename IteratorT>
	void set_rom(const IteratorT begin, const IteratorT end) {
		rom_ = std::vector<uint8_t>(begin, end);
		update_commutable_rom();
	}

	void set_cartridge(const std::vector<uint8_t> &cartridge) {
		cartridge_ = cartridge;
		if constexpr (!is_mo6) {
			cartridge_is_paged_ = true;
			rom_.clear();
		}
		update_commutable_rom();
	}

	void set_floppy_rom(const std::vector<uint8_t> &rom) {
		std::copy_n(rom.begin(), std::min(rom.size(), floppy_rom_.size()), floppy_rom_.begin());
		set_read(0xa, floppy_rom_.data());
	}

	uint8_t *video(const bool pixels) {
		return &video_[pixels ? 0x2000 : 0x0000];
	}

	// MARK: - Paging.

	void page_video(const bool pixels) {
		uint8_t *const base = video(pixels);
		set_readwrite(0x0, base);
		set_readwrite(0x1, base + 0x1000);
	}

	void page_ram() {
		// TODO: should be pageable.
		for(size_t c = 0x0; c < 0x8; c++) {
			set_readwrite(c + 0x2, ram_.data() + (c << 12));
		}
	}

	// MARK: - Memory Access.

	template <typename AddressT>
	uint8_t read(const AddressT address) const {
		if(read_[address >> 12]) {
			return read_[address >> 12][address];
		} else {
			return 0xff;
		}
	}

	template <typename AddressT>
	void write(const AddressT address, const uint8_t value) {
		if(write_[address >> 12]) {
			write_[address >> 12][address] = value;
		} else {
			Log::Logger<Log::Source::MO5>::info().append("Unmapped write of %02x to %04x", value, +address);
		}
	}

	// Defined to work correctly only for in-RAM addresses.
	uint8_t &operator[] (const size_t index) {
		return write_[index >> 12][index];
	}


	// MARK: - Register Access.

	template <uint16_t address>
	uint8_t read() {
		Log::Logger<Log::Source::MO5>::info().append("Unhandled read from %04x", address);
		return 0xff;
	}

	template <uint16_t address>
	void write(const uint8_t value) {
		Log::Logger<Log::Source::MO5>::info().append("Unhandled write of %02x to %04x", value, address);
	}

private:
	uint8_t *write_[0x10]{};
	const uint8_t *read_[0x10]{};

	void update_commutable_rom() {
		const auto set_range = [&](const size_t start, const size_t length, const uint8_t *source) {
			for(size_t c = 0x0; c < length; c++) {
				set_read(c + start, source + (c << 12));
			}
		};

		if(cartridge_is_paged_) {
			// TODO: there's some sort of paging here, too.
			set_range(0xb, 0x4, cartridge_.data());
			return;
		}

		switch(paged_rom_) {
			case PagedROM::FirstBank12kb:
				set_read(0xb, nullptr);
				set_range(0xc, 0x3, rom_.data());
			break;
			case PagedROM::SecondBank12kb:
				set_read(0xb, nullptr);
				set_range(0xc, 0x3, rom_.data() + 12*1024);
			break;
			case PagedROM::ThirdBank16kb:
				set_range(0xb, 0x4, rom_.data() + 28*1024);
			break;
			case PagedROM::FourthBank16kb:
				set_range(0xb, 0x4, rom_.data() + 44*1024);
			break;
		}
	}

	void set_read(const size_t slot, const uint8_t *const pointer) {
		read_[slot] = pointer ? pointer - (slot << 12) : nullptr;
		write_[slot] = nullptr;
	}
	void set_readwrite(const size_t slot, uint8_t *const pointer) {
		read_[slot] = write_[slot] = pointer ? pointer - (slot << 12) : nullptr;
	}

	std::array<uint8_t, 0x4000> video_;
	std::array<uint8_t, (is_mo6 ? (128 * 1024) : (32 * 1024))> ram_;
	std::array<uint8_t, 0x7c0> floppy_rom_;
	std::array<uint8_t, 0x1000> monitor_;
	std::vector<uint8_t> rom_;
	std::vector<uint8_t> cartridge_;

	// TODO: the following are entirely invented by me. Probably will need adjusting to
	// whatever the hardware registers model, if I ever figure it out.
	enum class PagedROM {
		FirstBank12kb = 0,
		SecondBank12kb = 1,
		ThirdBank16kb = 2,
		FourthBank16kb = 3,
	} paged_rom_ = is_mo6 ? PagedROM::SecondBank12kb : PagedROM::FirstBank12kb;
	bool cartridge_is_paged_ = false;
};

}
