//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "Nick.hpp"

#include "../MachineTypes.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Analyser/Static/Enterprise/Target.hpp"

#include "../../ClockReceiver/JustInTime.hpp"

namespace Enterprise {

/*
	Notes to self on timing:

	Nick divides each line into 57 windows; each window lasts 16 cycles and dedicates the
	first 10 of those to VRAM accesses, leaving the final six for a Z80 video RAM access
	if one has been requested.

	The Z80 has a separate, asynchronous 4Mhz clock. That's that.

	The documentation is also very forward in emphasising that Nick generates phaselocked
	(i.e. in-phase) PAL video.

	So: 57*16 = 912 cycles/line.

	A standard PAL line lasts 64µs and during that time outputs 283.7516 colour cycles.

	I shall _guess_ that the Enterprise stretches each line to 284 colour cycles rather than
	reducing it to 283.

	Therefore 912 cycles occurs in 284/283.7516 * 64 µs, which would appear to give an ideal
	clock rate of around:

		14,237,536.27 Hz.

	Given that there's always some leeway in a receiver, I'm modelling that as 14,237,536 cycles,
	which means that Nick runs 444923/125000 times as fast as the Z80. Which is around 3.56 times
	as fast.

	If that's true then the 6-cycle window is around 1.69 Z80 cycles long. Given that the Z80
	clock in an Enterprise can be stopped in half-cycle increments only, the Z80 can only be
	guaranteed to have around a 1.19 cycle minimum for its actual access. I'm therefore further
	postulating that the clock stoppage takes place so as to align the final cycle of a relevant
	access over the available window.

*/

class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine {
	public:
		ConcreteMachine([[maybe_unused]] const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this) {
			// Request a clock of 4Mhz; this'll be mapped upwards for Nick and Dave elsewhere.
			set_clock_rate(4'000'000);

			constexpr ROM::Name exos_name = ROM::Name::EnterpriseEXOS;
			const auto request = ROM::Request(exos_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &exos = roms.find(exos_name)->second;
			memcpy(exos_.data(), exos.data(), std::min(exos_.size(), exos.size()));

			// Take a reasonable guess at the initial memory configuration:
			// put EXOS into the first bank since this is a Z80 and therefore
			// starts from address 0; the third instruction in EXOS is a jump
			// to $c02e so it's reasonable to assume EXOS is in the highest bank
			// too, and it appears to act correctly if it's the first 16kb that's
			// in the highest bank. From there I guess: all banks are initialised
			// to 0.
			page<0>(0x00);
			page<1>(0x00);
			page<2>(0x00);
			page<3>(0x00);
		}

		// MARK: - Z80::BusHandler.
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			using PartialMachineCycle = CPU::Z80::PartialMachineCycle;
			const uint16_t address = cycle.address ? *cycle.address : 0x0000;

			// TODO: possibly apply an access penalty.
			nick_ += cycle.length;

			switch(cycle.operation) {
				default: break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch(address & 0xff) {
						default:
							printf("Unhandled input: %04x\n", address);
							assert(false);
						break;

						case 0xb0:	*cycle.value = pages_[0];	break;
						case 0xb1:	*cycle.value = pages_[1];	break;
						case 0xb2:	*cycle.value = pages_[2];	break;
						case 0xb3:	*cycle.value = pages_[3];	break;

						case 0xb4:
							printf("TODO: interrupt enable/reset read\n");
							*cycle.value = 0xff;
						break;
						case 0xb5:
							printf("TODO: Keyboard/joystick input\n");
							*cycle.value = 0xff;
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					switch(address & 0xff) {
						default:
							printf("Unhandled output: %04x\n", address);
							assert(false);
						break;

						case 0x80:	case 0x81:	case 0x82:	case 0x83:
						case 0x84:	case 0x85:	case 0x86:	case 0x87:
						case 0x88:	case 0x89:	case 0x8a:	case 0x8b:
						case 0x8c:	case 0x8d:	case 0x8e:	case 0x8f:
							nick_->write(address, *cycle.value);
						break;

						case 0xb0:	page<0>(*cycle.value);	break;
						case 0xb1:	page<1>(*cycle.value);	break;
						case 0xb2:	page<2>(*cycle.value);	break;
						case 0xb3:	page<3>(*cycle.value);	break;

						case 0xa0:	case 0xa1:	case 0xa2:	case 0xa3:
						case 0xa4:	case 0xa5:	case 0xa6:	case 0xa7:
						case 0xa8:	case 0xa9:	case 0xaa:	case 0xab:
						case 0xac:	case 0xad:	case 0xae:	case 0xaf:
							printf("TODO: audio adjust %04x <- %02x\n", address, *cycle.value);
						break;

						case 0xb4:
							printf("TODO: interrupt enable/reset write %02x\n", *cycle.value);
						break;
						case 0xb5:
							printf("TODO: Keyboard/etc %02x\n", *cycle.value);
						break;
						case 0xb6:
							printf("TODO: printer output %02x\n", *cycle.value);
						break;
						case 0xbf:
							printf("TODO: Dave sysconfig %02x\n", *cycle.value);
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Read:
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
					if(read_pointers_[address >> 14]) {
						*cycle.value = read_pointers_[address >> 14][address];
					} else {
						*cycle.value = 0xff;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(write_pointers_[address >> 14]) {
						write_pointers_[address >> 14][address] = *cycle.value;
					}
				break;
			}

			return HalfCycles(0);
		}

		void flush() {
			nick_.flush();
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;

		// MARK: - Memory layout
		std::array<uint8_t, 32 * 1024> exos_;
		std::array<uint8_t, 256 * 1024> ram_;
		const uint8_t min_ram_slot_ = 0xff - 3;

		const uint8_t *read_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
		uint8_t *write_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
		uint8_t pages_[4] = {0x80, 0x80, 0x80, 0x80};

		template <size_t slot> void page(uint8_t offset) {
			pages_[slot] = offset;

			if(offset < 2) {
				page<slot>(&exos_[offset * 0x4000], nullptr);
				return;
			}

			if(offset >= min_ram_slot_) {
				const size_t address = (offset - min_ram_slot_) * 0x4000;
				page<slot>(&ram_[address], &ram_[address]);
				return;
			}

			page<slot>(nullptr, nullptr);
		}

		template <size_t slot> void page(const uint8_t *read, uint8_t *write) {
			read_pointers_[slot] = read ? read - (slot * 0x4000) : nullptr;
			write_pointers_[slot] = write ? write - (slot * 0x4000) : nullptr;
		}

		// MARK: - ScanProducer
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - TimedMachine
		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		// MARK: - Video.
		JustInTimeActor<Nick, HalfCycles, 444923, 125000> nick_;
		// Cf. timing guesses above.
};

}

using namespace Enterprise;

Machine *Machine::Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target *const enterprise_target = dynamic_cast<const Target *>(target);

	return new Enterprise::ConcreteMachine(*enterprise_target, rom_fetcher);
}

Machine::~Machine() {}
