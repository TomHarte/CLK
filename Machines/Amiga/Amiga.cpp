//
//  Amiga.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Amiga.hpp"

#include "../MachineTypes.hpp"

#include "../../Processors/68000/68000.hpp"

#include "../../Analyser/Static/Amiga/Target.hpp"

#include "../Utility/MemoryPacker.hpp"
#include "../Utility/MemoryFuzzer.hpp"

namespace Amiga {

class ConcreteMachine:
	public CPU::MC68000::BusHandler,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Amiga::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this)
		{
			(void)target;

			// Temporary: use a hard-coded Kickstart selection.
			constexpr ROM::Name rom_name = ROM::Name::AmigaA500Kickstart13;
			ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(roms.find(rom_name)->second, kickstart_.data());

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
			set_region(0x00'0000, 0x08'00000, kickstart_.data(), CPU::MC68000::Microcycle::PermitRead);
			set_region(0xfc'0000, 0x1'00'0000, kickstart_.data(), CPU::MC68000::Microcycle::PermitRead);

			// NTSC clock rate: 2*3.579545 = 7.15909Mhz.
			// PAL clock rate: 7.09379Mhz.
			set_clock_rate(7'093'790.0);
		}

		// MARK: - MC68000::BusHandler.
		using Microcycle = CPU::MC68000::Microcycle;
		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int) {
			// Do nothing if no address is exposed.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// Otherwise, intended 1-2 step here is:
			//
			//	(1) determine when this CPU access will be scheduled;
			//	(2)	do all the other actions prior to this CPU access being scheduled.
			//
			// (or at least enqueue them, JIT-wise).

			// TODO: interrupt acknowledgement.

			// Grab the target address to pick a memory source.
			const uint32_t address = cycle.host_endian_byte_address();
			if(cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord)) {
				printf("%06x\n", address);
			}

			if(!regions_[address >> 18].read_write_mask) {
				if((cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord))) {
					// Check for various potential chip accesses.
					if(address >= 0xbf'd000 && address <= 0xbf'ef01) {
						printf("Unimplemented CIA %06x\n", address);
						assert(false);
					} else if(address >= 0xdf'f000 && address <= 0xdf'f1be) {
						printf("Unimplemented chipset access %06x\n", address);
						assert(false);
					} else {
						// This'll do for open bus, for now.
						cycle.set_value16(0xffff);
					}
				}
			} else {
				// A regular memory access.
				cycle.apply(
					&regions_[address >> 18].contents[address],
					regions_[address >> 18].read_write_mask
				);
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;

		// MARK: - Memory map.
		std::array<uint8_t, 512*1024> ram_;
		std::array<uint8_t, 512*1024> kickstart_;

		struct MemoryRegion {
			uint8_t *contents = nullptr;
			int read_write_mask = 0;
		} regions_[64];	// i.e. top six bits are used as an index.

		void set_region(int start, int end, uint8_t *base, int read_write_mask) {
			assert(!(start & ~0xfc'0000));
			assert(!((end - (1 << 18)) & ~0xfc'0000));

			for(int c = start >> 18; c < end >> 18; c++) {
				regions_[c].contents = base - (c << 18);
				regions_[c].read_write_mask = read_write_mask;
			}
		}

		// MARK: - MachineTypes::ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - MachineTypes::TimedMachine.

		void run_for(const Cycles cycles) {
			mc68000_.run_for(cycles);
		}
};

}


using namespace Amiga;

Machine *Machine::Amiga(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Amiga::Target;
	const Target *const amiga_target = dynamic_cast<const Target *>(target);
	return new Amiga::ConcreteMachine(*amiga_target, rom_fetcher);
}

Machine::~Machine() {}
