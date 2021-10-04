//
//  MemoryMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef MemoryMap_hpp
#define MemoryMap_hpp

namespace Amiga {

struct MemoryMap {
	public:
		std::array<uint8_t, 512*1024> chip_ram{};
		std::array<uint8_t, 512*1024> kickstart{0xff};

		struct MemoryRegion {
			uint8_t *contents = nullptr;
			unsigned int read_write_mask = 0;
		} regions[64];	// i.e. top six bits are used as an index.

		MemoryMap() {
			// Address spaces that matter:
			//
			//	00'0000 – 08'0000:	chip RAM.	[or overlayed KickStart]
			//	– 10'0000: extended chip ram for ECS.
			//	– 20'0000: auto-config space (/fast RAM).
			//	...
			//	bf'd000 – c0'0000: 8250s.
			//	c0'0000 – d8'0000: pseudo-fast RAM.
			//	...
			//	dc'0000 – dd'0000: optional real-time clock.
			//	df'f000 - e0'0000: custom chip registers.
			//	...
			//	f0'0000 — : 512kb Kickstart (or possibly just an extra 512kb reserved for hypothetical 1mb Kickstart?).
			//	f8'0000 — : 256kb Kickstart if 2.04 or higher.
			//	fc'0000 – : 256kb Kickstart otherwise.
			set_region(0xfc'0000, 0x1'00'0000, kickstart.data(), CPU::MC68000::Microcycle::PermitRead);
			reset();
		}

		void reset() {
			set_overlay(true);
		}

		void set_overlay(bool enabled) {
			if(overlay_ == enabled) {
				return;
			}
			overlay_ = enabled;

			if(enabled) {
				set_region(0x00'0000, 0x08'0000, kickstart.data(), CPU::MC68000::Microcycle::PermitRead);
			} else {
				// Mirror RAM to fill out the address range up to $20'0000.
				set_region(0x00'0000, 0x08'0000, chip_ram.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
				set_region(0x08'0000, 0x10'0000, chip_ram.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
				set_region(0x10'0000, 0x18'0000, chip_ram.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
				set_region(0x18'0000, 0x20'0000, chip_ram.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
			}
		}

	private:
		bool overlay_ = false;

		void set_region(int start, int end, uint8_t *base, unsigned int read_write_mask) {
			assert(!(start & ~0xfc'0000));
			assert(!((end - (1 << 18)) & ~0xfc'0000));

			base -= start;
			for(int c = start >> 18; c < end >> 18; c++) {
				regions[c].contents = base;
				regions[c].read_write_mask = read_write_mask;
			}
		}
};

}
#endif /* MemoryMap_hpp */
