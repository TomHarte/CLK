//
//  MemoryMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cassert>
#include <iterator>
#include <vector>

#include "Outputs/Log.hpp"

namespace Thomson::MO5 {

template <bool is_mo6>
struct MemoryMap {
public:
	MemoryMap() {
		// Install monitor at 0xf000.
		set_read(0xf, monitor_.data());

		// Install RAM.
		page_video(false);
		page_ram();
	}

	// MARK: - ROM installation.

	template <typename IteratorT>
	void set_monitor(const IteratorT begin, const IteratorT end) {
		assert(std::distance(begin, end) == 0x1000);
		std::copy_n(begin, end, monitor_.begin());
	}

	void set_rom(const std::vector<uint8_t> &rom) {
		rom_ = rom;
		update_commutable_rom();
	}

	// MARK: - Paging.

	void page_video(const bool pixels) {
		uint8_t *base = &video_[pixels ? 0x0000 : 0x2000];
		set_readwrite(0x0, base);
		set_readwrite(0x1, base + 0x1000);
	}

	void page_ram() {
		// TODO: should be pageable.
		for(size_t c = 0x3; c < 0xb; c++) {
			set_readwrite(c, ram_.data() + (c << 12));
		}
	}

	// MARK: - Access.

	template <typename AddressT>
	uint8_t read(const AddressT address) const {
		return read_[address >> 12][address];
	}

	template <typename AddressT>
	void write(const AddressT address, const uint8_t value) {
		if(write_[address >> 12]) {
			write_[address >> 12] = value;
		} else {
			Log::Logger<Log::Source::MO5>::info().append("Unmapped write of %02x to %04x", value, +address);
		}
	}

private:
	uint8_t *write_[0x10]{};
	const uint8_t *read_[0x10]{};

	void update_commutable_rom() {
		// TODO: this should be pageable.

		// Commutable ROM: 0xb000 to 0xf000.
		for(size_t c = 0xb; c < 0xf; c++) {
			set_read(c, rom_.begin() + (c << 12));
		}
	}

	void set_read(const size_t slot, const uint8_t *const pointer) {
		read_[slot] = pointer - (slot << 12);
		write_[slot] = nullptr;
	}
	void set_readwrite(const size_t slot, uint8_t *const pointer) {
		read_[slot] = write_[slot] = pointer - (slot << 12);
	}

	std::array<uint8_t, 0x2000 > video_;
	std::array<uint8_t, (is_mo6 ? (128 * 1024) : (48 * 1024))> ram_;
	std::array<uint8_t, 0x7c0> floppy_rom_;
	std::array<uint8_t, 0x1000> monitor_;
	std::vector<uint8_t> rom_;
};

}
