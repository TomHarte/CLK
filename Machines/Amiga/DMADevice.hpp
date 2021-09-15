//
//  DMADevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef DMADevice_hpp
#define DMADevice_hpp

#include <array>
#include <cstddef>
#include <cstdint>

namespace Amiga {

class Chipset;

class DMADeviceBase {
	public:
		DMADeviceBase(Chipset &chipset, uint16_t *ram, size_t size) :
			chipset_(chipset), ram_(ram), ram_mask_(uint32_t((size - 1) >> 1)) {}

	protected:
		Chipset &chipset_;
		uint16_t *const ram_ = nullptr;
		const uint32_t ram_mask_ = 0;
};

template <size_t num_addresses> class DMADevice: public DMADeviceBase {
	public:
		using DMADeviceBase::DMADeviceBase;

		/// Writes the word @c value to the address register @c id, shifting it by @c shift (0 or 16) first.
		template <int id, int shift> void set_pointer(uint16_t value) {
			static_assert(id < num_addresses);
			static_assert(shift == 0 || shift == 16);
			pointer_[id] = (pointer_[id] & (0xffff'0000 >> shift)) | uint32_t(value << shift);
		}

	protected:
		std::array<uint32_t, num_addresses> pointer_{};
};

}

#endif /* DMADevice_hpp */
