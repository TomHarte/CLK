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

enum class AccessMode {
	System = 0,
	LightPen = 1,
};

enum class GraphicsModeAccess {
	Authorised = 0,
	Inhibited = 1,
};

//
// Notes on the layout of the standard MO ROM images:
//
//	The MO5 images are 16kb in total, representing what should go into the
//	final 16kb of the memory map.
//
//	The MO6 images are 64kb in total:
//
//	* the first 32kb is the BASIC 1 portion: two 16kb pages that might appear from 0xc000, including the monitor;
//	* the next 32kb is the BASIC 128 portion: two 16kb pages that might appear from 0xb000 or might not appear at all.

template <bool is_mo6>
struct MemoryMap {
public:
	MemoryMap() {
		// Install RAM.
		page_video(false);

		// Put fixed RAM into memory.
		for(size_t c = 0x0; c < 0x4; c++) {
			set_readwrite(c + 0x2, ram_.data() + (c << 12));
		}
		update_commutable_ram();

		// Set RAM to an undefined state.
		Memory::Fuzz(ram_);
		Memory::Fuzz(video_);
	}

	// MARK: - ROM installation and connection for video.

	void set_rom(const std::vector<uint8_t> &rom) {
		rom_ = rom;
		update_commutable_rom();
	}

	void set_cartridge(const std::vector<uint8_t> &cartridge) {
		cartridge_ = cartridge;
		if constexpr (!is_mo6) {
			b000page_ = B000Page::Cartridge;
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

	void page_monitor(const bool page) {
		if(!is_mo6) {
			return;
		}

		if(rom_page_ != page) {
			rom_page_ = page;
			update_commutable_rom();
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
		switch(address) {
			default:
				Log::Logger<Log::Source::MO5>::info().append("Unhandled read from %04x", address);
			break;

			case 0xa7e4:
				if(access_mode_ == AccessMode::System) {
					// TODO: b6, b7: displayed RAM page number;
					return
						(cartridge_.empty() ? 0x00 : 0x20) |
						(basic_selection_ ? 0x10 : 0x00);
				}
			break;

			case 0xa7e5:
				if(access_mode_ == AccessMode::System) {
					return uint8_t(ram_page_);
				}
			break;

			case 0xa7e6:
				// This is possibly cartridge paging???
				if(access_mode_ == AccessMode::System) {
					return 0;
				}
			break;

			case 0xa7e7:
			return 0xfe | uint8_t(access_mode_);	// Other bits come from video.
		}

		return 0xff;
	}

	template <uint16_t address>
	void write(const uint8_t value) {
//		printf("%04x: %02x\n", address, value);

		switch(address) {
			default:
				Log::Logger<Log::Source::MO5>::info().append("Unhandled write of %02x to %04x", value, address);
			break;

			case 0xa7dd:
				// "The GATE MODE PAGE (IW18) HANDLES:
				//	- selection between the BASIC 1 [(IW01) EPROM 27256] or the BASIC 128
				//	[(IW02) EPROM 27256]. This is set on bit D4 of A7DD.
				//	- Masking or unmasking of optional ROM cartridges through bit D5 of A7DD.
				//	(refer to GATE MODE PAGE REGISTERS (page: 22)).

				basic_selection_ = value & 0x10;
				if(!(value & 0x20) && !cartridge_.empty()) {
					// TODO: The test on a cartridge being present is empirical;
					// figure out whether it's genuine or an unrelated stab in the dark.
					b000page_ = B000Page::Cartridge;
				} else if(basic_selection_) {
					b000page_ = B000Page::BASIC128;
				} else {
					b000page_ = B000Page::Empty;
				}
				update_commutable_rom();
			break;

			case 0xa7e4:
				access_mode_ = AccessMode(value & 1);
			break;

			case 0xa7e5:
				ram_page_ = value & 0b11111;
				update_commutable_ram();
			break;
		}
	}

	AccessMode access_mode() const {
		return access_mode_;
	}

private:
	uint8_t *write_[0x10]{};
	const uint8_t *read_[0x10]{};

	void update_commutable_ram() {
		const auto offset = ram_page_ * 16384;
		if(offset < ram_.size()) {
			uint8_t *const page = &ram_[offset];
			for(size_t c = 0x0; c < 0x4; c++) {
				set_readwrite(c + 0x6, page + (c << 12));
			}
		} else {
			for(size_t c = 0x0; c < 0x4; c++) {
				set_read(c + 0x6, nullptr);
			}
		}
	}

	void update_commutable_rom() {
		const auto set_16kbrange = [&](const size_t start, const uint8_t *source) {
			for(size_t c = 0; c < 4; c++) {
				set_read(c + start, source ? source + (c << 12) : nullptr);
			}
		};

		// Establish both correct monitor paging and a default of BASIC 1.
		set_16kbrange(0xc, rom_.data() + rom_page_ * 0x4000);

		switch(b000page_) {
			default:
				set_read(0xb, nullptr);
			break;

			case B000Page::Cartridge:
				if(cartridge_.empty()) {
					set_16kbrange(0xb, nullptr);
				} else {
					// TODO: there's some sort of internal paging here, too.
					set_16kbrange(0xb, cartridge_.data());
				}
			break;

			case B000Page::BASIC128: {
				const uint8_t *const basic128 = rom_.data() + 0x8000;
				set_16kbrange(0xb, basic128 + rom_page_ * 0x4000);
			} break;
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
	std::vector<uint8_t> rom_;
	std::vector<uint8_t> cartridge_;

	size_t rom_page_ = 0;
	size_t ram_page_ = 1;
	bool basic_selection_ = false;
	enum class B000Page {
		Empty,
		BASIC128,
		Cartridge,
	} b000page_ = B000Page::Empty;

	AccessMode access_mode_ = AccessMode::LightPen;
};

}
