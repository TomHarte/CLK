//
//  Plus4.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Plus4.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Processors/6502/6502.hpp"

#include "../../../Analyser/Static/Commodore/Target.hpp"

using namespace Commodore::Plus4;

namespace {

template <typename AddressT, typename DataT, int NumPages>
class Pager {
public:
	DataT read(AddressT address) {
		return read_[address >> Shift][address];
	}
	DataT &write(AddressT address) {
		return write_[address >> Shift][address];
	}

	template <int slot>
	void page(const uint8_t *read, uint8_t *write) {
		write_[slot] = write - (slot << Shift);
		read_[slot] = read - (slot << Shift);
	}

private:
	std::array<DataT *, NumPages> write_{};
	std::array<const DataT *, NumPages> read_{};

	static constexpr auto AddressBits = sizeof(AddressT) * 8;
	static constexpr auto PageSize = (1 << AddressBits) / NumPages;
	static_assert(!(PageSize & (PageSize - 1)), "Pages must be a power of two in size");

	static constexpr int ln2(int value) {
		int result = 0;
		while(value != 1) {
			value >>= 1;
			++result;
		}
		return result;
	}
	static constexpr auto Shift = ln2(PageSize);
};

class ConcreteMachine:
	public CPU::MOS6502::BusHandler,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::MediaTarget,
	public Machine {
public:
	ConcreteMachine(const Analyser::Static::Commodore::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6502_(*this)
	{
		const auto kernel = ROM::Name::Plus4KernelPALv5;
		const auto basic = ROM::Name::Plus4BASIC;

		const ROM::Request request = ROM::Request(basic) && ROM::Request(kernel);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		kernel_ = roms.find(kernel)->second;
		basic_ = roms.find(basic)->second;

		map_.page<0>(ram_.data(), ram_.data());
		map_.page<1>(ram_.data() + 16*1024, ram_.data() + 16*1024);
		map_.page<2>(basic_.data(), ram_.data() + 32*1024);
		map_.page<3>(kernel_.data(), ram_.data() + 48*1024);

		insert_media(target.media);
	}

	Cycles perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		(void)operation;
		(void)address;
		(void)value;

		return Cycles(1);
	}

private:
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, true> m6502_;

	void set_scan_target(Outputs::Display::ScanTarget *const) final {
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return {};
	}

	void run_for(const Cycles cycles) final {
		m6502_.run_for(cycles);
	}

	bool insert_media(const Analyser::Static::Media &) final {
		return true;
	}

	Pager<uint16_t, uint8_t, 4> map_;
	std::array<uint8_t, 65536> ram_;
	std::vector<uint8_t> kernel_;
	std::vector<uint8_t> basic_;
};

}
std::unique_ptr<Machine> Machine::Plus4(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Commodore::Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return std::make_unique<ConcreteMachine>(*commodore_target, rom_fetcher);
}
