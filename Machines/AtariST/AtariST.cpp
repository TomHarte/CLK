//
//  AtariST.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "AtariST.hpp"

#include "../CRTMachine.hpp"

namespace {

const int CLOCK_RATE = 8000000;

using Target = Analyser::Static::Target;

class ConcreteMachine:
	public Atari::ST::Machine,
	public CRTMachine::Machine {
	public:
		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
			set_clock_rate(CLOCK_RATE);
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return nullptr;
		}

		void run_for(const Cycles cycles) final {
		}
};

}

using namespace Atari::ST;

Machine *Machine::AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
