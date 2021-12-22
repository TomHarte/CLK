//
//  MemoryMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef MemoryMap_hpp
#define MemoryMap_hpp

#include "../../Analyser/Static/Amiga/Target.hpp"

namespace Amiga {

class MemoryMap {
	private:
		static constexpr auto PermitRead = CPU::MC68000::Microcycle::PermitRead;
		static constexpr auto PermitWrite = CPU::MC68000::Microcycle::PermitWrite;
		static constexpr auto PermitReadWrite = PermitRead | PermitWrite;

	public:
		// TODO: decide what of the below I want to be dynamic.
		std::array<uint8_t, 512*1024> kickstart{0xff};
		std::array<uint8_t, 1024*1024> chip_ram{};

		struct MemoryRegion {
			uint8_t *contents = nullptr;
			unsigned int read_write_mask = 0;
		} regions[64];	// i.e. top six bits are used as an index.

		using FastRAM = Analyser::Static::Amiga::Target::FastRAM;
		MemoryMap(FastRAM fast_ram_size) {
			// Address spaces that matter:
			//
			//	00'0000 – 08'0000:	chip RAM.	[or overlayed KickStart]
			//	– 10'0000: extended chip ram for ECS.
			//	– 20'0000: slow RAM and further chip RAM.
			//	– a0'0000: auto-config space (/fast RAM).
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
			set_region(0xfc'0000, 0x1'00'0000, kickstart.data(), PermitRead);

			switch(fast_ram_size) {
				default:
					fast_autoconf_visible_ = false;
				break;
				case FastRAM::OneMegabyte:
					fast_ram_.resize(1 * 1024 * 1024);
					fast_ram_size_ = 5;
				break;
				case FastRAM::TwoMegabytes:
					fast_ram_.resize(2 * 1024 * 1024);
					fast_ram_size_ = 6;
				break;
				case FastRAM::FourMegabytes:
					fast_ram_.resize(4 * 1024 * 1024);
					fast_ram_size_ = 7;
				break;
				case FastRAM::EightMegabytes:
					fast_ram_.resize(8 * 1024 * 1024);
					fast_ram_size_ = 0;
				break;
			}

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

			set_region(0x00'0000, uint32_t(chip_ram.size()), chip_ram.data(), PermitReadWrite);
			if(enabled) {
				set_region(0x00'0000, 0x08'0000, kickstart.data(), PermitRead);
			}
		}

		/// Performs the provided microcycle, which the caller guarantees to be a memory access,
		/// and in the Zorro register range.
		bool perform(const CPU::MC68000::Microcycle &cycle) {
			if(!fast_autoconf_visible_) return false;

			const uint32_t register_address = *cycle.address & 0xfe;

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.operation & Microcycle::Read) {
				// Re: Autoconf:
				//
				// "All read registers physically return only the top 4 bits of data, on D31-D28";
				// (this is from Zorro III documentation; I'm assuming it to be D15–D11 for the
				// 68000's 16-bit bus);
				//
				// "Every AUTOCONFIG register is logically considered to be 8 bits wide; the
				// 8 bits actually being nybbles from two paired addresses."

				uint8_t value = 0xf;
				switch(register_address) {
					default: break;

					case 0x00:	// er_Type (high)
						value =
							0xc |	// Zoro II-style PIC.
							0x2;	// Memory will be linked into the free pool
					break;
					case 0x02:	// er_Type (low)
						value = fast_ram_size_;
					break;

					// er_Manufacturer
					//
					// On the manufacturer number: this is supposed to be assigned
					// by Commodore. TODO: find and crib a real fast RAM number, if it matters.
					//
					// (0xffff seems to be invalid, so _something_ needs to be supplied)
					case 0x10:	case 0x12:
						value = 0xa; // Manufacturer's number, high byte.
					break;
					case 0x14:	case 0x16:
						value = 0xb;		// Manufacturer's number, low byte.
					break;
				}

				// Shove the value into the top of the data bus.
				cycle.set_value16(uint16_t(0x0fff | (value << 12)));
			} else {
				fast_autoconf_visible_ &= !(register_address >= 0x4c && register_address < 0x50);

				switch(register_address) {
					default: break;

					case 0x48: {	// ec_BaseAddress (A23–A16)
						const auto address = uint32_t(cycle.value8_high()) << 16;
						set_region(address, uint32_t(address + fast_ram_.size()), fast_ram_.data(), PermitRead | PermitWrite);
						fast_autoconf_visible_ = false;
					} break;
				}
			}

			return true;
		}

	private:
		std::vector<uint8_t> fast_ram_{};
		uint8_t fast_ram_size_ = 0;

		bool fast_autoconf_visible_ = true;
		bool overlay_ = false;

		void set_region(uint32_t start, uint32_t end, uint8_t *base, unsigned int read_write_mask) {
			[[maybe_unused]] constexpr uint32_t precision_loss_mask = uint32_t(~0xfc'0000);
			assert(!(start & precision_loss_mask));
			assert(!((end - (1 << 18)) & precision_loss_mask));
			assert(end > start);

			if(base) base -= start;
			for(decltype(start) c = start >> 18; c < end >> 18; c++) {
				regions[c].contents = base;
				regions[c].read_write_mask = read_write_mask;
			}
		}
};

}
#endif /* MemoryMap_hpp */
