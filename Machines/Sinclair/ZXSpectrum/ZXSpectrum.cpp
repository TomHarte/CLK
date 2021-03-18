//
//  ZXSpectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrum.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Processors/Z80/Z80.hpp"

#include "../../../Analyser/Static/ZXSpectrum/Target.hpp"

#include <array>

namespace {
	const unsigned int ClockRate = 3'500'000;
}


namespace Sinclair {
namespace ZXSpectrum {

using Model = Analyser::Static::ZXSpectrum::Target::Model;
template<Model model> class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public CPU::Z80::BusHandler {
	public:
		ConcreteMachine(const Analyser::Static::ZXSpectrum::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this)
		{
			set_clock_rate(ClockRate);

			// With only the +2a and +3 currently supported, the +3 ROM is always
			// the one required.
			const auto roms =
				rom_fetcher({ {"ZXSpectrum", "the +2a/+3 ROM", "plus3.rom", 64 * 1024, 0x96e3c17a} });
			if(!roms[0]) throw ROMMachine::Error::MissingROMs;
			memcpy(rom_.data(), roms[0]->data(), std::min(rom_.size(), roms[0]->size()));

			// Set up initial memory map.
			update_memory_map();

			// TODO: insert media.
			(void)target;
		}

		// MARK: - TimedMachine

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		// MARK: - ScanProducer

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			// TODO.
			return Outputs::Display::ScanStatus();
		}

		// MARK: - BusHandler

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			(void)cycle;

			return HalfCycles(0);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;

		// MARK: - Memory.
		std::array<uint8_t, 64*1024> rom_;
		std::array<uint8_t, 128*1024> ram_;

		std::array<uint8_t, 16*1024> scratch_;
		const uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];

		uint8_t port1ffd_ = 0;
		uint8_t port7ffd_ = 0;
		bool disable_paging_ = false;

		void update_memory_map() {
			if(disable_paging_) {
				// Set 48kb-esque memory map.
				set_memory(0, rom_.data(), nullptr);
				set_memory(1, &ram_[5 * 16384], &ram_[5 * 16384]);
				set_memory(2, &ram_[2 * 16384], &ram_[2 * 16384]);
				set_memory(3, &ram_[0 * 16384], &ram_[0 * 16384]);
				return;
			}

			if(port1ffd_ & 1) {
				// "Special paging mode", i.e. one of four fixed
				// RAM configurations, port 7ffd doesn't matter.

				switch(port1ffd_ & 0x6) {
					default:
					case 0x00:
						set_memory(0, &ram_[0 * 16384], &ram_[0 * 16384]);
						set_memory(1, &ram_[1 * 16384], &ram_[1 * 16384]);
						set_memory(2, &ram_[2 * 16384], &ram_[2 * 16384]);
						set_memory(3, &ram_[3 * 16384], &ram_[3 * 16384]);
					break;

					case 0x02:
						set_memory(0, &ram_[4 * 16384], &ram_[4 * 16384]);
						set_memory(1, &ram_[5 * 16384], &ram_[5 * 16384]);
						set_memory(2, &ram_[6 * 16384], &ram_[6 * 16384]);
						set_memory(3, &ram_[7 * 16384], &ram_[7 * 16384]);
					break;

					case 0x04:
						set_memory(0, &ram_[4 * 16384], &ram_[4 * 16384]);
						set_memory(1, &ram_[5 * 16384], &ram_[5 * 16384]);
						set_memory(2, &ram_[6 * 16384], &ram_[6 * 16384]);
						set_memory(3, &ram_[3 * 16384], &ram_[3 * 16384]);
					break;

					case 0x06:
						set_memory(0, &ram_[4 * 16384], &ram_[4 * 16384]);
						set_memory(1, &ram_[7 * 16384], &ram_[7 * 16384]);
						set_memory(2, &ram_[6 * 16384], &ram_[6 * 16384]);
						set_memory(3, &ram_[3 * 16384], &ram_[3 * 16384]);
					break;
				}

				return;
			}

			// Apply standard 128kb-esque mapping (albeit with extra ROM to pick from).
			const auto rom = &rom_[ (((port1ffd_ >> 1) & 2) | ((port7ffd_ >> 4) & 1)) * 16384];
			set_memory(0, rom, nullptr);

			set_memory(1, &ram_[5 * 16384], &ram_[5 * 16384]);
			set_memory(2, &ram_[2 * 16384], &ram_[2 * 16384]);

			const auto high_ram = &ram_[(port7ffd_ & 7) * 16384];
			set_memory(3, high_ram, high_ram);
		}

		void set_memory(int bank, const uint8_t *read, uint8_t *write) {
			read_pointers_[bank] = read - bank*16384;
			write_pointers_[bank] = (write ? write : scratch_.data()) - bank*16384;
		}
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
