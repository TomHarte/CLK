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
#include <vector>

#include "../AppleII/LanguageCardSwitches.hpp"
#include "../AppleII/AuxiliaryMemorySwitches.hpp"

namespace Apple {
namespace IIgs {


class MemoryMap {
	public:
		// MARK: - Initial construction and configuration.

		MemoryMap() : auxiliary_switches_(*this), language_card_(*this) {}

		void set_storage(std::vector<uint8_t> &ram, std::vector<uint8_t> &rom) {
			// Keep a pointer for later.
			ram_ = ram.data();

			// Establish bank mapping.
			uint8_t next_region = 0;
			auto region = [&next_region, this]() -> uint8_t {
				assert(next_region != regions.size());
				return next_region++;
			};
			auto set_region = [this](uint8_t bank, uint16_t start, uint16_t end, uint8_t region) {
				assert((end == 0xffff) || !(end&0xff));
				assert(!(start&0xff));

				// Fill in memory map.
				size_t target = size_t((bank << 8) | (start >> 8));
				for(int c = start; c < end; c += 0x100) {
					region_map[target] = region;
					++target;
				}
			};
			auto set_regions = [this, set_region, region](uint8_t bank, std::initializer_list<uint16_t> addresses, std::vector<uint8_t> allocated_regions = {}) {
				uint16_t previous = 0x0000;
				auto next_region = allocated_regions.begin();
				for(uint16_t address: addresses) {
					set_region(bank, previous, address, next_region != allocated_regions.end() ? *next_region : region());
					previous = address;
					assert(next_region != allocated_regions.end() || allocated_regions.empty());
					if(next_region != allocated_regions.end()) ++next_region;
				}
				assert(next_region == allocated_regions.end());
			};

			// Current beliefs about the IIgs memory map:
			//
			//	* language card banking applies to banks $00, $01, $e0 and $e1;
			//	* auxiliary memory switches apply to banks $00 only;
			//	* shadowing may be enabled only on banks $00 and $01, or on all RAM pages.
			//
			// So banks $00 and $01 need their own divided spaces at the shadowing resolution,
			// all the other fast RAM banks can share a set of divided spaces, $e0 and $e1 need
			// to be able to deal with language card-level division but no further, and the pure
			// ROM pages don't need to be subdivided at all.

			// Reserve region 0 as that for unmapped memory.
			region();

			// Bank $00: all locations potentially affected by the auxiliary switches or the
			// language switches. Which will naturally align with shadowable zones.
			set_regions(0x00, {
				0x0200,	0x0400,	0x0800, 0x0c00,
				0x2000,	0x4000,
				0xc000,	0xc100,	0xc300,	0xc400,	0xc800,
				0xd000,	0xe000,
				0xffff
			});

			// Bank $01: all locations potentially affected by the language switches, by shadowing,
			// or marked for IO.
			set_regions(0x01, {
				0x0400,	0x0800, 0x0c00,
				0x2000,	0x4000, 0x6000, 0xa000,
				0xc000,		/* I don't think ROM-over-$Cx00 works in bank $01? */
				0xd000,	0xe000,
				0xffff
			});

			// Banks $02–[end of RAM]: all locations potentially affected by shadowing.
			const uint8_t fast_ram_bank_count = uint8_t((ram.size() - 128*1024) / 65536);
			if(fast_ram_bank_count > 2) {
				const std::vector<uint8_t> evens = {
					region(),	// 0x0000 – 0x0400.
					region(),	// 0x0400 – 0x0800.
					region(),	// 0x0800 – 0x0c00.
					region(),	// 0x0c00 – 0x2000.
					region(),	// 0x2000 – 0x4000.
					region(),	// 0x4000 – 0x6000.
					region(),	// 0x6000 – [end].
				};
				const std::vector<uint8_t> odds = {
					region(),	// 0x0000 – 0x0400.
					region(),	// 0x0400 – 0x0800.
					region(),	// 0x0800 – 0x0c00.
					region(),	// 0x0c00 – 0x2000.
					region(),	// 0x2000 – 0x4000.
					region(),	// 0x4000 – 0x6000.
					region(),	// 0x6000 – 0xa000.
					region(),	// 0xa000 – [end].
				};
				for(uint8_t bank = 0x02; bank < fast_ram_bank_count; bank += 2) {
					set_regions(bank,	{0x0400, 0x0800, 0x0c00, 0x2000, 0x4000, 0x6000, 0xffff}, evens);
					set_regions(bank+1,	{0x0400, 0x0800, 0x0c00, 0x2000, 0x4000, 0x6000, 0xa000, 0xffff}, odds);
				}
			}

			// [Banks $80–$e0: empty].

			// Banks $e0, $e1: all locations potentially affected by the language switches or marked for IO.
			// TODO: do I need to break up the Cx pages?
			for(uint8_t c = 0; c < 2; c++) {
				set_regions(0xe0 + c, {0xc000, 0xd000, 0xe000, 0xffff});
			}

			// [Banks $e2–[ROM start]: empty].

			// ROM banks: directly mapped to ROM.
			const uint8_t rom_bank_count = uint8_t(rom.size() >> 16);
			const uint8_t first_rom_bank = uint8_t(0x100 - rom_bank_count);
			const uint8_t rom_region = region();
			for(uint8_t c = 0; c < rom_bank_count; ++c) {
				set_region(first_rom_bank + c, 0x0000, 0xffff, rom_region);
			}

			// Apply proper storage to those banks.
			auto set_storage = [this](uint32_t address, const uint8_t *read, uint8_t *write) {
				// Don't allow the reserved null region to be modified.
				assert(region_map[address >> 8]);

				// Either set or apply a quick bit of testing as to the logic at play.
				auto &region = regions[region_map[address >> 8]];
				if(read) read -= address;
				if(write) write -= address;
				if(!region.read) {
					region.read = read;
					region.write = write;
				} else {
					assert(region.read == read);
					assert(region.write == write);
				}
			};

			// This is highly redundant, but decouples this step from the above.
			for(size_t c = 0; c < 0x800000; c += 0x100) {
				if(c < ram.size() - 128*1024) {
					set_storage(uint32_t(c), &ram[c], &ram[c]);
				}
			}
			uint8_t *const slow_ram = &ram[ram.size() - 0x20000];
			for(size_t c = 0xe00000; c < 0xe20000; c += 0x100) {
				set_storage(uint32_t(c), &slow_ram[c - 0xe00000], &slow_ram[c - 0xe00000]);
			}
			for(uint32_t c = 0; c < uint32_t(rom_bank_count); c++) {
				set_storage((first_rom_bank + c) << 16, &rom[c << 16], nullptr);
			}

			// Apply initial language/auxiliary state. [TODO: including shadowing register].
			set_card_paging();
			set_zero_page_paging();
			set_main_paging();
		}

		// MARK: - Live bus access notifications.

		void set_shadow_register(uint8_t value) {
			const uint8_t diff = value ^ shadow_register_;
			shadow_register_ = value;

			if(diff & 0x40) {	// IO/language-card inhibit
				set_language_card_paging();
				set_card_paging();
			}

			if(diff & 0x3f) {
				set_shadowing();
			}
		}

		void set_speed_register(uint8_t value) {
			const uint8_t diff = value ^ speed_register_;
			speed_register_ = value;
			if(diff & 0x10) {
				set_shadowing();
			}
		}

	private:
		Apple::II::AuxiliaryMemorySwitches<MemoryMap> auxiliary_switches_;
		Apple::II::LanguageCardSwitches<MemoryMap> language_card_;
		friend Apple::II::AuxiliaryMemorySwitches<MemoryMap>;
		friend Apple::II::LanguageCardSwitches<MemoryMap>;
		uint8_t *ram_ = nullptr;

		uint8_t shadow_register_ = 0x08;
		uint8_t speed_register_ = 0x00;

		// MARK: - Memory banking.

		void set_language_card_paging() {
			const auto language_state = language_card_.state();
			const auto zero_state = auxiliary_switches_.zero_state();
			const bool inhibit_banks0001 = shadow_register_ & 0x40;

			// Crib the ROM pointer from a page it's always visible on.
			const uint8_t *const rom = &regions[region_map[0xffd0]].read[0xffd000] - 0xd000;
			auto apply = [&language_state, &zero_state, rom, this](uint32_t bank_base, uint8_t *ram) {
				// All references below are to 0xc000, 0xd000 and 0xe000 but should
				// work regardless of bank.

				// TODO: verify order of ternary here — on the plain Apple II it was arbitrary.
				uint8_t *const lower_ram_bank = ram - (language_state.bank1 ? 0x0000 : 0x1000);

				auto &d0_region = regions[region_map[bank_base | 0xd0]];
				d0_region.read = language_state.read ? lower_ram_bank : rom;
				d0_region.write = language_state.write ? nullptr : lower_ram_bank;

				auto &e0_region = regions[region_map[bank_base | 0xe0]];
				e0_region.read = language_state.read ? ram : rom;
				e0_region.write = language_state.write ? nullptr : ram;

				// Assert assumptions made above re: memory layout.
				assert(region_map[bank_base | 0xd0] + 1 == region_map[bank_base | 0xe0]);
				assert(region_map[bank_base | 0xe0] == region_map[bank_base | 0xff]);
			};
			auto set_no_card = [this](uint32_t bank_base) {
				auto &d0_region = regions[region_map[bank_base | 0xd0]];
				d0_region.read = ram_;
				d0_region.write = ram_;

				auto &e0_region = regions[region_map[bank_base | 0xe0]];
				e0_region.read = ram_;
				e0_region.write = ram_;
			};

			if(inhibit_banks0001) {
				set_no_card(0x0000);
				set_no_card(0x0100);
			} else {
				apply(0x0000, zero_state ? &ram_[0x10000] : ram_);
				apply(0x0100, ram_);
			}

			uint8_t *const e0_ram = regions[region_map[0xe000]].write;
			apply(0xe000, e0_ram);
			apply(0xe100, e0_ram);
		}

		void set_card_paging() {
			const bool inhibit_banks0001 = shadow_register_ & 0x40;
			const auto state = auxiliary_switches_.card_state();

			// TODO: all work.

			if(inhibit_banks0001) {
				// Set no IO anywhere, all the Cx regions point to regular RAM
				// (or possibly auxiliary).
			} else {
				// Obey the card state for banks $00 and $01.
			}

			// Obey the card state for banks $e0 and $e1.
			(void)state;
		}

		void set_zero_page_paging() {
			// Affects bank $00 only, and should be a single region.
			auto &region = regions[region_map[0]];
			region.read = region.write = auxiliary_switches_.zero_state() ? &ram_[0x10000] : ram_;

			// Switching to or from auxiliary RAM potentially affects the language
			// and regular card areas.
			set_card_paging();
			set_language_card_paging();
		}

		void set_shadowing() {
		}

		void set_main_paging() {
			const auto state = auxiliary_switches_.main_state();

#define set(page, flags)	{\
			auto &region = regions[region_map[page]];	\
			region.read = flags.read ? &ram_[0x10000] : ram_;	\
			region.write = flags.write ? &ram_[0x10000] : ram_;	\
		}

			set(0x02, state.base);
			set(0x08, state.base);
			set(0x40, state.base);
			set(0x04, state.region_04_08);
			set(0x20, state.region_20_40);

#undef set
			// This also affects shadowing flags, if shadowing is enabled at all.
			set_shadowing();
		}

	public:
		// Memory layout here is done via double indirection; the main loop should:
		//	(i) use the top two bytes of the address to get an index from memory_map_; and
		//	(ii) use that to index the memory_regions table.
		//
		// Pointers are eight bytes at the time of writing, so the extra level of indirection
		// reduces what would otherwise be a 1.25mb table down to not a great deal more than 64kb.
		std::array<uint8_t, 65536> region_map;

		struct Region {
			uint8_t *write = nullptr;
			const uint8_t *read = nullptr;
			uint8_t flags = 0;

			enum Flag: uint8_t {
				IsShadowedE0 = 1 << 0,	// i.e. writes should also be written to bank $e0, and costed appropriately.
				IsShadowedE1 = 1 << 1,	// i.e. writes should also be written to bank $e1, and costed appropriately.
				IsShadowed = IsShadowedE0 | IsShadowedE1,
				Is1Mhz = 1 << 2,		// Both reads and writes should be synchronised with the 1Mhz clock.
				IsIO = 1 << 3,			// Indicates that this region should be checked for soft switches, registers, etc.
			};
		};
		std::array<Region, 64> regions;	// The assert above ensures that this is large enough; there's no
										// doctrinal reason for it to be whatever size it is now, just
										// adjust as required.
};

#define MemoryMapRegion(map, address) map.regions[map.region_map[address >> 8]]
#define MemoryMapRead(region, address, value) *value = region.read ? region.read[address] : 0xff
#define MemoryMapWrite(map, region, address, value) \
	if(region.write) {	\
		region.write[address] = *value;	\
		if(region.flags & (MemoryMap::Region::IsShadowed)) {	\
			const uint32_t shadowed_address = (address & 0xffff) + (uint32_t(0xe1 - (region.flags & MemoryMap::Region::IsShadowedE0)) << 16);	\
			map.regions[map.region_map[shadowed_address >> 8]].write[shadowed_address] = *value;	\
		}	\
	}

}
}

#endif /* MemoryMap_h */
