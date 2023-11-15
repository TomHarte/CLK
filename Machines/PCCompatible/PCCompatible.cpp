//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"
#include "../../InstructionSets/x86/Instruction.hpp"
#include "../../InstructionSets/x86/Perform.hpp"

#include "../TimedMachine.hpp"

namespace PCCompatible {

class ConcreteMachine:
	public Machine,
	public MachineTypes::TimedMachine
{
	public:
		ConcreteMachine(
			[[maybe_unused]] const Analyser::Static::Target &target,
			[[maybe_unused]] const ROMMachine::ROMFetcher &rom_fetcher
		) {
			// This is actually a MIPS count; try 3 million.
			set_clock_rate(3'000'000);

			// Fetch the BIOS. [8088 only, for now]
			ROM::Request request = ROM::Request(ROM::Name::PCCompatibleGLaBIOS);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
		}

		void run_for([[maybe_unused]] const Cycles cycles) override {}

	private:
};


}

using namespace PCCompatible;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::PCCompatible(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new PCCompatible::ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
