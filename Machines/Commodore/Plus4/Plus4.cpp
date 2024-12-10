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

class Timers {
public:
	template <int offset>
	void write(const uint8_t value) {
		const auto load_low = [&](uint16_t &target) {
			target = uint16_t((target & 0xff00) | (value << 0));
		};
		const auto load_high = [&](uint16_t &target) {
			target = uint16_t((target & 0x00ff) | (value << 8));
		};

		constexpr auto timer = offset >> 1;
		paused_[timer] = !(offset & 1);
		if constexpr (offset & 1) {
			load_high(timers_[timer]);
			if(!timer) {
				load_high(timer0_reload_);
			}
		} else {
			load_low(timers_[timer]);
			if(!timer) {
				load_low(timer0_reload_);
			}
		}
	}

	template <int offset>
	uint8_t read() {
		constexpr auto timer = offset >> 1;
		if constexpr (offset & 1) {
			return uint8_t(timers_[timer] >> 8);
		} else {
			return uint8_t(timers_[timer] >> 0);
		}
	}

	void tick(int count) {
		// Quick hack here; do better than stepping through one at a time.
		while(count--) {
			decrement<0>();
			decrement<1>();
			decrement<2>();
		}
	}

private:
	template <int timer>
	void decrement() {
		if(paused_[timer]) return;

		// Check for reload.
		if(!timer && !timers_[timer]) {
			timers_[timer] = timer0_reload_;
		}

		-- timers_[timer];

		// Check for interrupt.
		if(!timers_[timer]) {
		}
	}

	uint16_t timers_[3]{};
	uint16_t timer0_reload_ = 0xffff;
	bool paused_[3]{};
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
		// PAL: 8'867'240 divided by 5 or 4?
		// NTSC: 7'159'090?
		// i.e. colour subcarriers multiplied by two?

		set_clock_rate(8'867'240);	// TODO.

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
		// TODO: calculate length of this bus operation.
		const auto length = Cycles(5);

		// Update other subsystems.
		// TODO: timers decrement at a 894 KHz rate for NTSC television systems, 884 KHZ for PAL systems.
		// Probably a function of the speed register?
		timers_subcycles_ += length;
		const auto timers_cycles = timers_subcycles_.divide(Cycles(5));
		timers_.tick(timers_cycles.as<int>());

		// Perform actual access.
		if(address >= 0xff00 && address < 0xff40) {
			if(isReadOperation(operation)) {
				switch(address) {
					case 0xff00:	*value = timers_.read<0>();	break;
					case 0xff01:	*value = timers_.read<1>();	break;
					case 0xff02:	*value = timers_.read<2>();	break;
					case 0xff03:	*value = timers_.read<3>();	break;
					case 0xff04:	*value = timers_.read<4>();	break;
					case 0xff05:	*value = timers_.read<5>();	break;
				}
			} else {
				switch(address) {
					case 0xff00:	timers_.write<0>(*value);	break;
					case 0xff01:	timers_.write<1>(*value);	break;
					case 0xff02:	timers_.write<2>(*value);	break;
					case 0xff03:	timers_.write<3>(*value);	break;
					case 0xff04:	timers_.write<4>(*value);	break;
					case 0xff05:	timers_.write<5>(*value);	break;
				}
			}
		}

		if(address >= 0xfd00 && address < 0xff40) {
			if(isReadOperation(operation)) {
				printf("TODO: TED read @ %04x\n", address);
			} else {
				printf("TODO: TED write of %02x @ %04x\n", *value, address);
			}
			return length;
		}

		if(isReadOperation(operation)) {
			*value = map_.read(address);
		} else {
			map_.write(address) = *value;
		}

		return length;
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

	Cycles timers_subcycles_;
	Timers timers_;
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
