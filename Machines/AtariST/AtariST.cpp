//
//  AtariST.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "AtariST.hpp"

#include "../CRTMachine.hpp"

#include "../../Processors/68000/68000.hpp"

#include "Video.hpp"
#include "../../ClockReceiver/JustInTime.hpp"

namespace Atari {
namespace ST {

const int CLOCK_RATE = 8000000;

using Target = Analyser::Static::Target;

class ConcreteMachine:
	public Atari::ST::Machine,
	public CPU::MC68000::BusHandler,
	public CRTMachine::Machine {
	public:
		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this) {
			set_clock_rate(CLOCK_RATE);

			ram_.resize(512 * 1024);

			std::vector<ROMMachine::ROM> rom_descriptions = {
				{"AtariST", "the TOS ROM", "tos100.img", 192*1024, 0x1a586c64}
			};
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];
		}

		// MARK: CRTMachine::Machine
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_->set_scan_target(scan_target);
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return nullptr;
		}

		void run_for(const Cycles cycles) final {
			mc68000_.run_for(cycles);
		}

		// MARK: MC68000::BusHandler
		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		JustInTimeActor<Video, HalfCycles> video_;

		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;

};

}
}

using namespace Atari::ST;

Machine *Machine::AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
