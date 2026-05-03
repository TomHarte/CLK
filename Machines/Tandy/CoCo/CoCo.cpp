//
//  CoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCo.hpp"

#include "Processors/6809/6809.hpp"

#include "Machines/MachineTypes.hpp"
#include "Analyser/Static/TandyCoCo/Target.hpp"

using namespace Tandy::CoCo;

namespace {

struct MemoryMap {

	uint8_t read(const uint16_t address) {
		return read_[address >> 13] ? read_[address >> 13][address] : 0xff;
	}

	void write(const uint16_t address, uint8_t value) {
		if(!write_[address >> 13]) return;
		write_[address >> 13][address] = value;
	}

	void set_read(const uint16_t begin, const uint16_t end, const uint8_t *data) {
		assert(!(begin & 0x1fff));
		assert(!(end & 0x1fff));
		for(int page = begin >> 13; page < end >> 13; page++) {
			read_[page] = data - begin;
		}
	}

	void set_readwrite(const uint16_t begin, const uint16_t end, uint8_t *data) {
		assert(!(begin & 0x1fff));
		assert(!(end & 0x1fff));
		for(int page = begin >> 13; page < end >> 13; page++) {
			read_[page] = write_[page] = data - begin;
		}
	}

private:
	uint8_t *write_[8]{};
	const uint8_t *read_[8]{};
};

}

namespace TandyCoCo {

class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine
{
public:
	ConcreteMachine(const Analyser::Static::TandyCoCo::Target &, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6809_(*this)
	{
		set_clock_rate(1'789'772.5);

		const auto BasicROM = ROM::Name::TandyCoCoColourBasic10;
		auto request = ROM::Request(BasicROM);

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		{
			auto rom = roms.find(BasicROM)->second;
			std::copy_n(rom.begin(), 8 * 1024, colour_basic_.begin());
		}

		memory_.set_readwrite(0x0000, 0x8000, ram_.data());
		memory_.set_read(0xa000, 0xc000, colour_basic_.data());
	}

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::LIC lic,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		if(address >> 8 == 0xff) {
			if constexpr (read_write != CPU::M6809::ReadWrite::NoData) {
				switch(address) {
					default: printf("Unhandled at %04x\n", +address);	break;

					case 0xfff2:	case 0xfff3:	case 0xfff4:	case 0xfff5:
					case 0xfff6:	case 0xfff7:	case 0xfff8:	case 0xfff9:
					case 0xfffa:	case 0xfffb:	case 0xfffc:	case 0xfffd:
					case 0xfffe:	case 0xffff:
						if constexpr (CPU::M6809::is_read(read_write)) {
							value = memory_.read(address - 0x4000);
						}
					break;
				}
			}
		} else {
			if constexpr (CPU::M6809::is_read(read_write)) {
				value = memory_.read(address);
			}

			if constexpr (CPU::M6809::is_write(read_write)) {
				memory_.write(address, value);
			}
		}

		// TODO: only if operating at half speed, obviously.
		return duration<Cycles>(bus_phase);
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;
	MemoryMap memory_;
	std::array<uint8_t, 8 * 1024> colour_basic_;
	std::array<uint8_t, 64 * 1024> ram_;

	// MARK: - ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		(void)target;
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	void flush_output(const int outputs) final {
		(void)outputs;
	}
};

}

std::unique_ptr<Machine> Machine::create(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const auto &coco_target = static_cast<const Analyser::Static::TandyCoCo::Target &>(target);
	return std::make_unique<TandyCoCo::ConcreteMachine>(coco_target, rom_fetcher);
}
