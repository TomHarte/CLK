//
//  ZXSpectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrum.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Analyser/Static/ZXSpectrum/Target.hpp"

#include <array>

namespace {
	const unsigned int ClockRate = 3'500'000;
}


namespace Sinclair {
namespace ZXSpectrum {

using Model = Analyser::Static::ZXSpectrum::Target::Model;
template<Model model> class ConcreteMachine:
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::ZXSpectrum::Target &target, const ROMMachine::ROMFetcher &rom_fetcher)
		{
			set_clock_rate(ClockRate);

			// With only the +2a and +3 currently supported, the +3 ROM is always
			// the one required.
			const auto roms =
				rom_fetcher({ {"ZXSpectrum", "the +2a/+3 ROM", "plus3.rom", 64 * 1024, 0x96e3c17a} });
			if(!roms[0]) throw ROMMachine::Error::MissingROMs;
			memcpy(rom_.data(), roms[0]->data(), std::min(rom_.size(), roms[0]->size()));

			// TODO: insert media, set up memory map.
			(void)target;
		}

		// MARK: - TimedMachine

		void run_for(const Cycles cycles) override {
			// TODO.
			(void)cycles;
		}

		// MARK: - ScanProducer

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			// TODO.
			return Outputs::Display::ScanStatus();
		}

	private:
		std::array<uint8_t, 64*1024> rom_;
		std::array<uint8_t, 128*1024> ram_;
};


}
}

using namespace Sinclair::ZXSpectrum;

Machine *Machine::ZXSpectrum(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const auto zx_target = dynamic_cast<const Analyser::Static::ZXSpectrum::Target *>(target);

	switch(zx_target->model) {
		case Model::Plus2a:	return new ConcreteMachine<Model::Plus2a>(*zx_target, rom_fetcher);
		case Model::Plus3:	return new ConcreteMachine<Model::Plus3>(*zx_target, rom_fetcher);
	}

	return nullptr;
}

Machine::~Machine() {}
