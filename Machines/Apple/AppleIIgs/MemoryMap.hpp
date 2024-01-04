//
//  MemoryMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Machines_Apple_AppleIIgs_MemoryMap_hpp
#define Machines_Apple_AppleIIgs_MemoryMap_hpp

#include <array>
#include <bitset>
#include <vector>

#include "../AppleII/LanguageCardSwitches.hpp"
#include "../AppleII/AuxiliaryMemorySwitches.hpp"

namespace Apple::IIgs {

class MemoryMap {
	public:
		// MARK: - Initial construction and configuration.

		MemoryMap(bool is_rom03) : auxiliary_switches_(*this), language_card_(*this) {
			setup_shadow_maps(is_rom03);
		}

		/// Sets the ROM and RAM storage underlying this MemoryMap.
		void set_storage(std::vector<uint8_t> &ram, std::vector<uint8_t> &rom);

		// MARK: - Live bus access notifications and register access.

		void set_shadow_register(uint8_t value);
		uint8_t get_shadow_register() const;

		void set_speed_register(uint8_t value);

		void set_state_register(uint8_t value);
		uint8_t get_state_register() const;

		void access(uint16_t address, bool is_read);

		using AuxiliaryMemorySwitches = Apple::II::AuxiliaryMemorySwitches<MemoryMap>;
		const AuxiliaryMemorySwitches &auxiliary_switches() const {
			return auxiliary_switches_;
		}

		using LanguageCardSwitches = Apple::II::LanguageCardSwitches<MemoryMap>;
		const LanguageCardSwitches &language_card_switches() const {
			return language_card_;
		}

		// MARK: - Accessors for reading and writing RAM.

		struct Region {
			uint8_t *write = nullptr;
			const uint8_t *read = nullptr;
			uint8_t flags = 0;

			enum Flag: uint8_t {
				Is1Mhz = 1 << 0,		// Both reads and writes should be synchronised with the 1Mhz clock.
				IsIO = 1 << 1,			// Indicates that this region should be checked for soft switches, registers, etc.
			};
		};

		const Region &region(uint32_t address) const {	return regions_[region_map_[address >> 8]];	}
		uint8_t read(const Region &region, uint32_t address) const {
			return region.read ? region.read[address] : 0xff;
		}

		bool is_shadowed(const Region &region, uint32_t address) const {
			const auto physical = physical_address(region, address);
			assert(physical >= 0 && physical <= 0xff'ffff);
			return shadow_pages_[(physical >> 10) & 127] & shadow_banks_[physical >> 17];
		}
		void write(const Region &region, uint32_t address, uint8_t value) {
			if(!region.write) {
				return;
			}

			// Write once.
			region.write[address] = value;

			// Write again, either to the same place (if unshadowed) or to the shadow destination.
			static constexpr std::size_t shadow_mask[2] = {0xff'ffff, 0x01'ffff};
			const bool shadowed = is_shadowed(region, address);
			shadow_base_[shadowed][physical_address(region, address) & shadow_mask[shadowed]] = value;
		}

		// The objective is to support shadowing:
		//	1. without storing a whole extra pointer, and such that the shadowing flags
		//		are orthogonal to the current auxiliary memory settings;
		//	2. in such a way as to support shadowing both in banks $00/$01 and elsewhere; and
		//	3. to do so without introducing too much in the way of branching.
		//
		// Hence the implemented solution: if shadowing is enabled then use the distance from the start of
		// physical RAM modulo 128k indexed into the bank $e0/$e1 RAM.
		//
		// With a further twist: the modulo and pointer are indexed on ::IsShadowed to eliminate a branch
		// even on that.

	private:
		AuxiliaryMemorySwitches auxiliary_switches_;
		LanguageCardSwitches language_card_;
		friend AuxiliaryMemorySwitches;
		friend LanguageCardSwitches;

		uint8_t shadow_register_ = 0x00;
		uint8_t speed_register_ = 0x00;

		// MARK: - Banking.

		void assert_is_region(uint8_t start, uint8_t end);
		template <int type> void set_paging();

		uint8_t *ram_base_ = nullptr;

		// Memory layout here is done via double indirection; the main loop should:
		//	(i) use the top two bytes of the address to get an index from region_map; and
		//	(ii) use that to index the memory_regions table.
		//
		// Pointers are eight bytes at the time of writing, so the extra level of indirection
		// reduces what would otherwise be a 1.25mb table down to not a great deal more than 64kb.
		std::array<uint8_t, 65536> region_map_{};
		std::array<Region, 40> regions_;	// An assert above ensures that this is large enough; there's no
											// doctrinal reason for it to be whatever size it is now, just
											// adjust as required.

		std::size_t physical_address(const Region &region, uint32_t address) const {
			return std::size_t(&region.write[address] - ram_base_);
		}

		// MARK: - Shadowing

		// Various precomputed bitsets describing key regions; std::bitset doesn't support constexpr instantiation
		// beyond the first 64 bits at the time of writing, alas, so these are generated at runtime.
		std::bitset<128> shadow_text1_;
		std::bitset<128> shadow_text2_;
		std::bitset<128> shadow_highres1_, shadow_highres1_aux_;
		std::bitset<128> shadow_highres2_, shadow_highres2_aux_;
		std::bitset<128> shadow_superhighres_;
		void setup_shadow_maps(bool is_rom03);
		void set_shadowing();

		uint8_t *shadow_base_[2] = {nullptr, nullptr};

		// Divide the final 128kb of memory into 1kb chunks and flag to indicate whether
		// each is a potential destination for shadowing.
		std::bitset<128> shadow_pages_{};

		// Divide the whole 16mb of memory into 128kb chunks and flag to indicate whether
		// each is a potential source of shadowing.
		std::bitset<128> shadow_banks_{};
};

}

#endif /* MemoryMap_h */
