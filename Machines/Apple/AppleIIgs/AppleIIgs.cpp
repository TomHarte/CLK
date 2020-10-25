//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Processors/65816/65816.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"

#include "../AppleII/LanguageCardSwitches.hpp"
#include "../AppleII/AuxiliaryMemorySwitches.hpp"

#include <cassert>
#include <array>

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Apple::IIgs::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this),
			auxiliary_switches_(*this),
			language_card_(*this) {

			set_clock_rate(14318180.0);

			using Target = Analyser::Static::AppleIIgs::Target;
			std::vector<ROMMachine::ROM> rom_descriptions;
			const std::string machine_name = "AppleIIgs";
			switch(target.model) {
				case Target::Model::ROM00:
					/* TODO */
				case Target::Model::ROM01:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0);
				break;

				case Target::Model::ROM03:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29);
				break;
			}
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];

			size_t ram_size = 0;
			switch(target.memory_model) {
				case Target::MemoryModel::TwoHundredAndFiftySixKB:
					ram_size = 256;
				break;

				case Target::MemoryModel::OneMB:
					ram_size = 128 + 1024;
				break;

				case Target::MemoryModel::EightMB:
					ram_size = 128 + 8 * 1024;
				break;
			}
			ram_.resize(ram_size * 1024);

			// Establish bank mapping.
			uint8_t next_region = 0;
			auto region = [&next_region, this]() -> uint8_t {
				assert(next_region != memory_regions_.size());
				return next_region++;
			};
			auto set_region = [this](uint8_t bank, uint16_t start, uint16_t end, uint8_t region) {
				assert((end == 0xffff) || !(end&0xff));
				assert(!(start&0xff));

				// Fill in memory map.
				size_t target = size_t((bank << 8) | (start >> 8));
				for(int c = start; c < end; c += 0x100) {
					memory_map_[target] = region;
					++target;
				}
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
			set_region(0x00, 0x0000, 0x0200, region());
			set_region(0x00, 0x0200, 0x0400, region());
			set_region(0x00, 0x0400, 0x0800, region());
			set_region(0x00, 0x0800, 0x2000, region());
			set_region(0x00, 0x2000, 0x4000, region());
			set_region(0x00, 0x4000, 0xc000, region());
			set_region(0x00, 0xc000, 0xc100, region());
			set_region(0x00, 0xc100, 0xc300, region());
			set_region(0x00, 0xc300, 0xc400, region());
			set_region(0x00, 0xc400, 0xc800, region());
			set_region(0x00, 0xc800, 0xd000, region());
			set_region(0x00, 0xd000, 0xe000, region());
			set_region(0x00, 0xe000, 0xffff, region());

			// Bank $01: all locations potentially affected by the language switches, by shadowing,
			// or marked for IO.
			set_region(0x01, 0x0000, 0x0400, region());
			set_region(0x01, 0x0400, 0x0800, region());
			set_region(0x01, 0x0800, 0x0c00, region());
			set_region(0x01, 0x0c00, 0x2000, region());
			set_region(0x01, 0x2000, 0x4000, region());
			set_region(0x01, 0x4000, 0x6000, region());
			set_region(0x01, 0x6000, 0xa000, region());
			set_region(0x01, 0xa000, 0xc000, region());
			set_region(0x01, 0xc000, 0xd000, region());
			set_region(0x01, 0xd000, 0xe000, region());
			set_region(0x01, 0xe000, 0xffff, region());

			// Banks $02–[end of RAM]: all locations potentially affected by shadowing.
			const uint8_t fast_ram_bank_count = uint8_t((ram_size - 128)/64);
			if(fast_ram_bank_count > 2) {
				const uint8_t evens[] = {
					region(),	// 0x0000 – 0x0400.
					region(),	// 0x0400 – 0x0800.
					region(),	// 0x0800 – 0x0c00.
					region(),	// 0x0c00 – 0x2000.
					region(),	// 0x2000 – 0x4000.
					region(),	// 0x4000 – 0x6000.
					region(),	// 0x6000 – [end].
				};
				const uint8_t odds[] = {
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
					set_region(bank, 0x0000, 0x0400, evens[0]);
					set_region(bank, 0x0400, 0x0800, evens[1]);
					set_region(bank, 0x0800, 0x0c00, evens[2]);
					set_region(bank, 0x0c00, 0x2000, evens[3]);
					set_region(bank, 0x2000, 0x4000, evens[4]);
					set_region(bank, 0x4000, 0x6000, evens[5]);
					set_region(bank, 0x6000, 0xffff, evens[6]);

					set_region(bank+1, 0x0000, 0x0400, odds[0]);
					set_region(bank+1, 0x0400, 0x0800, odds[1]);
					set_region(bank+1, 0x0800, 0x0c00, odds[2]);
					set_region(bank+1, 0x0c00, 0x2000, odds[3]);
					set_region(bank+1, 0x2000, 0x4000, odds[4]);
					set_region(bank+1, 0x4000, 0x6000, odds[5]);
					set_region(bank+1, 0x6000, 0xa000, odds[6]);
					set_region(bank+1, 0xa000, 0xffff, odds[7]);
				}
			}

			// [Banks $80–$e0: empty].

			// Banks $e0, $e1: all locations potentially affected by the language switches or marked for IO.
			for(uint8_t c = 0; c < 2; c++) {
				set_region(0xe0 + c, 0x0000, 0xc000, region());
				set_region(0xe0 + c, 0xc000, 0xd000, region());
				set_region(0xe0 + c, 0xd000, 0xffff, region());
			}

			// [Banks $e2–[ROM start]: empty].

			// ROM banks: directly mapped to ROM.
			const uint8_t rom_bank_count = uint8_t(rom_.size() >> 16);
			const uint8_t first_rom_bank = uint8_t(0x100 - rom_bank_count);
			const uint8_t rom_region = region();
			for(uint8_t c = 0; c < rom_bank_count; ++c) {
				set_region(first_rom_bank + c, 0x0000, 0xff00, rom_region);
			}

			// Apply proper storage to those banks.
			auto set_storage = [this](uint32_t address, const uint8_t *read, uint8_t *write) {
				// Don't allow the reserved null region to be modified.
				assert(memory_map_[address >> 8]);

				// Either set or apply a quick bit of testing as to the logic at play.
				auto &region = memory_regions_[memory_map_[address >> 8]];
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
				if(c < (ram_size - 128)*1024) {
					set_storage(uint32_t(c), &ram_[c], &ram_[c]);
				}
			}
			uint8_t *const slow_ram = &ram_[ram_.size() - 0x20000];
			for(size_t c = 0xe00000; c < 0xe20000; c += 0x100) {
				set_storage(uint32_t(c), &slow_ram[c - 0xe00000], &slow_ram[c - 0xe00000]);
			}
			for(uint32_t c = 0; c < uint32_t(rom_bank_count); ++c) {
				set_storage((first_rom_bank + c) << 16, &rom_[c << 16], &rom_[c << 16]);
			}

			// Apply initial language/auxiliary state. [TODO: including shadowing register].
			set_card_paging();
			set_zero_page_paging();
			set_main_paging();
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void set_scan_target(Outputs::Display::ScanTarget *) override {
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const MemoryRegion &region = memory_regions_[memory_map_[address >> 8]];

			if(region.flags & MemoryRegion::IsIO) {
				// TODO: all IO accesses.
			} else {
				// TODO: branching below is predicated on the idea that an extra 64kb of scratch write area
				// and 64kb of 0xffs would be worse than branching due to the data set increase. Verify that?
				if(isReadOperation(operation)) {
					*value = region.read ? region.read[address] : 0xff;
				} else {
					if(region.write) {
						region.write[address] = *value;

						// Apply shadowing.
						if(region.flags & (MemoryRegion::IsShadowedE0|MemoryRegion::IsShadowedE1)) {
							const uint32_t shadowed_address = (address & 0xffff) + (uint32_t(0xe1 - (region.flags&MemoryRegion::IsShadowedE0)) << 16);
							memory_regions_[memory_map_[shadowed_address >> 8]].write[shadowed_address] = *value;
						}
					}
				}
			}

			Cycles duration = Cycles(5);

			// TODO: determine the cost of this access.
//			if((mapping.flags & BankMapping::Is1Mhz) || ((mapping.flags & BankMapping::IsShadowed) && !isReadOperation(operation))) {
//				// TODO: (i) get into phase; (ii) allow for the 1Mhz bus length being sporadically 16 rather than 14.
//				duration = Cycles(14);
//			} else {
//				// TODO: (i) get into phase; (ii) allow for collisions with the refresh cycle.
//				duration = Cycles(5);
//			}
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 5;		// TODO: modulo something else, to allow for refresh.
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 14;	// TODO: modulo something else, to allow for stretched cycles.
			return duration;
		}

		// MARK: - Memory banking.
		void set_language_card_paging() {
		}
		void set_card_paging() {
		}
		void set_zero_page_paging() {
			set_language_card_paging();
		}
		void set_main_paging() {
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
		Apple::II::AuxiliaryMemorySwitches<ConcreteMachine> auxiliary_switches_;
		Apple::II::LanguageCardSwitches<ConcreteMachine> language_card_;

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		// MARK: - Memory layout and storage.

		// Memory layout here is done via double indirection; the main loop should:
		//	(i) use the top two bytes of the address to get an index from memory_map_; and
		//	(ii) use that to index the memory_regions_ table.
		//
		// Pointers are eight bytes at the time of writing, so the extra level of indirection
		// reduces what would otherwise be a 1.25mb table down to not a great deal more than 64kb.
		std::array<uint8_t, 65536> memory_map_;

		struct MemoryRegion {
			uint8_t *write = nullptr;
			const uint8_t *read = nullptr;
			uint8_t flags = 0;

			enum Flag: uint8_t {
				IsShadowedE0 = 1 << 0,	// i.e. writes should also be written to bank $e0, and costed appropriately.
				IsShadowedE1 = 1 << 1,	// i.e. writes should also be written to bank $e1, and costed appropriately.
				Is1Mhz = 1 << 2,		// Both reads and writes should be synchronised with the 1Mhz clock.
				IsIO = 1 << 3,			// Indicates that this region should be checked for soft switches, registers, etc.
			};
		};
		std::array<MemoryRegion, 47> memory_regions_;	// The assert above ensures that this is large enough; there's no
														// doctrinal reason for it to be whatever size it is now, just
														// adjust as required.

		// MARK: - Memory storage.

		// Actual memory storage.
		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}
