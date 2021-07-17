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
			(void)rom_fetcher;

			// Temporary: use a hard-coded Kickstart selection.
			constexpr ROM::Name rom_name = ROM::Name::AmigaA500Kickstart13;
			ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(roms.find(rom_name)->second, kickstart_);

			// NTSC clock rate: 2*3.579545 = 7.15909Mhz.
			// PAL clock rate: 7.09379Mhz.
			set_clock_rate(7'093'790.0);
		}

		// MARK: - MC68000::BusHandler.
		using Microcycle = CPU::MC68000::Microcycle;
		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			(void)cycle;
			(void)is_supervisor;

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		std::vector<uint8_t> kickstart_;

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
