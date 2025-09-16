//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Machines/MachineTypes.hpp"

#include "Processors/6502/6502.hpp"

#include "Analyser/Static/Acorn/Target.hpp"
#include "Outputs/Log.hpp"

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>

namespace BBCMicro {

class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) :
		m6502_(*this)
	{
		set_clock_rate(2'000'000);

		// Grab ROMs.
		using Request = ::ROM::Request;
		using Name = ::ROM::Name;
		const auto request = Request(Name::AcornBASICII) && Request(Name::BBCMicroMOS12);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto os_data = roms.find(Name::BBCMicroMOS12)->second;
		std::copy(os_data.begin(), os_data.end(), os_.begin());

		install_sideways(15, roms.find(Name::AcornBASICII)->second, false);

		// Setup fixed parts of memory map.
		page(0, &ram_[0], true);
		page(1, &ram_[1], true);
		page_sideways(15);
		page(3, os_.data(), true);

		(void)target;
	}

	// MARK: - 6502 bus.
	Cycles perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		static constexpr auto is_1mhz = [](const uint16_t address) {
			if(address < 0xfe00 || address >= 0xff00) return false;
			// TODO: 1Mhz should apply only for "most of the SHEILA ($FExx) devices, except for the Econet, floppy,
			// Tube, VIDPROC, and memory mapping registers."
			return true;
		};

		// Determine whether this access hits the 1Mhz bus; if so then apply appropriate penalty,
		// and update phase.
		const auto duration = is_1mhz(address) ? Cycles(2 + (phase_&1)) : Cycles(1);
		phase_ += duration.as<int>();

		// TODO: advance subsystems.

		// Check for an IO access; if found then perform that and exit.
		if(address >= 0xfc00 && address < 0xff00) {
			Logger::error().append("Unhandled IO access at %04x", address);
			return duration;
		}

		// ROM or RAM access.
		if(is_read(operation)) {
			*value = memory_[address >> 14][address];
		} else {
			if(memory_write_masks_[address >> 14]) {
				memory_[address >> 14][address] = *value;
			}
		}

		return duration;
	}

private:
	using Logger = Log::Logger<Log::Source::BBCMicro>;

	// MARK: - ScanProducer.
	void set_scan_target(Outputs::Display::ScanTarget *) override {}
	Outputs::Display::ScanStatus get_scan_status() const override {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.
	void run_for(const Cycles cycles) override {
		m6502_.run_for(cycles);
	}

	// MARK: - Clock phase.
	int phase_ = 0;

	// MARK: - Memory.
	std::array<uint8_t, 32 * 1024> ram_;
	using ROM = std::array<uint8_t, 16 * 1024>;
	ROM os_;
	std::array<ROM, 16> roms_;

	std::bitset<16> rom_inserted_;
	std::bitset<16> rom_write_masks_;

	uint8_t *memory_[4];
	std::bitset<4> memory_write_masks_;
	bool sideways_read_mask_ = false;
	void page(const size_t slot, uint8_t *const source, bool is_writeable) {
		memory_[slot] = source - (slot * 16384);
		memory_write_masks_[slot] = is_writeable;
	}

	void page_sideways(const size_t source) {
		sideways_read_mask_ = rom_inserted_[source];
		page(2, roms_[source].data(), rom_write_masks_[source]);
	}

	void install_sideways(const size_t slot, const std::vector<uint8_t> &source, bool is_writeable) {
		rom_write_masks_[slot] = is_writeable;

		assert(source.size() == roms_[slot].size());
		std::copy(source.begin(), source.end(), roms_[slot].begin());
	}

	// MARK: - Components.
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;
};

}

using namespace BBCMicro;

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	return std::make_unique<BBCMicro::ConcreteMachine>(*acorn_target, rom_fetcher);
}
