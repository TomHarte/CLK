//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "Keyboard.hpp"
#include "Nick.hpp"
#include "EXDos.hpp"

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

	Therefore 912 cycles occurs in 284/283.7516 * 64 µs.

	So one line = 181760000 / 2837516 µs = 45440000 / 709379 µs
	=> one cycle = 45440000 / 709379*912 = 45440000 / 646953648 = 2840000 / 40434603 µs
	=> clock rate of 40434603 / 2840000 Mhz

	And, therefore, the ratio to a 4Mhz Z80 clock is:

		40434603 / (2840000 * 4)
		= 40434603 / 11360000
		i.e. roughly 3.55 Nick cycles per Z80 cycle.

	If that's true then the 6-cycle window is around 1.69 Z80 cycles long. Given that the Z80
	clock in an Enterprise can be stopped in half-cycle increments only, the Z80 can only be
	guaranteed to have around a 1.19 cycle minimum for its actual access. I'm therefore further
	postulating that the clock stoppage takes place so as to align the final cycle of a relevant
	access over the available window.

*/

template <bool has_disk_controller> class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine {
	public:
		ConcreteMachine([[maybe_unused]] const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this),
			nick_(ram_.end() - 65536) {
			// Request a clock of 4Mhz; this'll be mapped upwards for Nick and Dave elsewhere.
			set_clock_rate(4'000'000);

			ROM::Request request;
			using Target = Analyser::Static::Enterprise::Target;

			// Pick one or more EXOS ROMs.
			switch(target.exos_version) {
				case Target::EXOSVersion::v10:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS10);	break;
				case Target::EXOSVersion::v20:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS20);	break;
				case Target::EXOSVersion::v21:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS21);	break;
				case Target::EXOSVersion::v23:	request = request && ROM::Request(ROM::Name::EnterpriseEXOS23);	break;
				case Target::EXOSVersion::Any:
					request =
						request && (
							ROM::Request(ROM::Name::EnterpriseEXOS10) || ROM::Request(ROM::Name::EnterpriseEXOS20) ||
							ROM::Request(ROM::Name::EnterpriseEXOS21) || ROM::Request(ROM::Name::EnterpriseEXOS23)
						);
				break;

				default: break;
			}

			// Similarly pick one or more BASIC ROMs.
			switch(target.basic_version) {
				case Target::BASICVersion::v10:
					request = request && (
						ROM::Request(ROM::Name::EnterpriseBASIC10) ||
						(ROM::Request(ROM::Name::EnterpriseBASIC10Part1) && ROM::Request(ROM::Name::EnterpriseBASIC10Part2))
					);
				break;
				case Target::BASICVersion::v11:
					request = request && (
						ROM::Request(ROM::Name::EnterpriseBASIC11) ||
						ROM::Request(ROM::Name::EnterpriseBASIC11Suffixed)
					);
				case Target::BASICVersion::v21:
					request = request && ROM::Request(ROM::Name::EnterpriseBASIC21);
				break;
				case Target::BASICVersion::Any:
					request =
						request && (
							ROM::Request(ROM::Name::EnterpriseBASIC10) ||
							(ROM::Request(ROM::Name::EnterpriseBASIC10Part1) && ROM::Request(ROM::Name::EnterpriseBASIC10Part2)) ||
							ROM::Request(ROM::Name::EnterpriseBASIC11) ||
							ROM::Request(ROM::Name::EnterpriseBASIC21)
						);
				break;

				default: break;
			}

			// Possibly add in a DOS.
			switch(target.dos) {
				case Target::DOS::EPDOS:	request = request && ROM::Request(ROM::Name::EnterpriseEPDOS);	break;
				case Target::DOS::EXDOS:	request = request && ROM::Request(ROM::Name::EnterpriseEXDOS);	break;
				default: break;
			}

			// Get and validate ROMs.
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			// Extract the appropriate EXOS ROM.
			exos_.fill(0xff);
			for(const auto rom_name: { ROM::Name::EnterpriseEXOS10, ROM::Name::EnterpriseEXOS20, ROM::Name::EnterpriseEXOS21, ROM::Name::EnterpriseEXOS23 }) {
				const auto exos = roms.find(rom_name);
				if(exos != roms.end()) {
					memcpy(exos_.data(), exos->second.data(), std::min(exos_.size(), exos->second.size()));
					break;
				}
			}

			// Extract the appropriate BASIC ROM[s] (if any).
			basic_.fill(0xff);
			bool has_basic = false;
			for(const auto rom_name: { ROM::Name::EnterpriseBASIC10, ROM::Name::EnterpriseBASIC11, ROM::Name::EnterpriseBASIC11Suffixed, ROM::Name::EnterpriseBASIC21 }) {
				const auto basic = roms.find(rom_name);
				if(basic != roms.end()) {
					memcpy(basic_.data(), basic->second.data(), std::min(basic_.size(), basic->second.size()));
					has_basic = true;
					break;
				}
			}
			if(!has_basic) {
				const auto basic1 = roms.find(ROM::Name::EnterpriseBASIC10Part1);
				const auto basic2 = roms.find(ROM::Name::EnterpriseBASIC10Part2);
				if(basic1 != roms.end() && basic2 != roms.end()) {
					memcpy(&basic_[0x0000], basic1->second.data(), std::min(size_t(8192), basic1->second.size()));
					memcpy(&basic_[0x2000], basic2->second.data(), std::min(size_t(8192), basic2->second.size()));
				}
			}

			// Extract the appropriate DOS ROM.
			dos_.fill(0xff);
			for(const auto rom_name: { ROM::Name::EnterpriseEPDOS, ROM::Name::EnterpriseEXDOS }) {
				const auto dos = roms.find(rom_name);
				if(dos != roms.end()) {
					memcpy(dos_.data(), dos->second.data(), std::min(dos_.size(), dos->second.size()));
					break;
				}
			}

			// Seed key state.
			clear_all_keys();

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

			// Pass on any media.
			insert_media(target.media);
		}

		// MARK: - Z80::BusHandler.
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			using PartialMachineCycle = CPU::Z80::PartialMachineCycle;
			const uint16_t address = cycle.address ? *cycle.address : 0x0000;

			// TODO: possibly apply an access penalty.


			if(nick_ += cycle.length) {
				const auto nick = nick_.last_valid();
				const bool nick_interrupt_line = nick->get_interrupt_line();
				if(nick_interrupt_line && !previous_nick_interrupt_line_) {
					set_interrupt(Interrupt::Nick, nick_.last_sequence_point_overrun());
				}
				previous_nick_interrupt_line_ = nick_interrupt_line;
			}

			// The WD/etc runs at a nominal 8Mhz.
			if constexpr (has_disk_controller) {
				exdos_.run_for(Cycles(cycle.length.as_integral()));
			}

			switch(cycle.operation) {
				default: break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch(address & 0xff) {
						default:
							printf("Unhandled input: %04x\n", address);
//							assert(false);
							*cycle.value = 0xff;
						break;

						case 0x10:	case 0x11:	case 0x12:	case 0x13:
						case 0x14:	case 0x15:	case 0x16:	case 0x17:
							*cycle.value = exdos_.read(address);
						break;
						case 0x18:	case 0x19:	case 0x1a:	case 0x1b:
						case 0x1c:	case 0x1d:	case 0x1e:	case 0x1f:
							*cycle.value = exdos_.get_control_register();
						break;

						case 0xb0:	*cycle.value = pages_[0];	break;
						case 0xb1:	*cycle.value = pages_[1];	break;
						case 0xb2:	*cycle.value = pages_[2];	break;
						case 0xb3:	*cycle.value = pages_[3];	break;

						case 0xb4:
							*cycle.value = interrupt_mask_ | interrupt_state_;
						break;
						case 0xb5:
							if(active_key_line_ < key_lines_.size()) {
								*cycle.value = key_lines_[active_key_line_];
							} else {
								*cycle.value = 0xff;
							}
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					switch(address & 0xff) {
						default:
							printf("Unhandled output: %04x\n", address);
//							assert(false);
						break;

						case 0x10:	case 0x11:	case 0x12:	case 0x13:
						case 0x14:	case 0x15:	case 0x16:	case 0x17:
							exdos_.write(address, *cycle.value);
						break;
						case 0x18:	case 0x19:	case 0x1a:	case 0x1b:
						case 0x1c:	case 0x1d:	case 0x1e:	case 0x1f:
							 exdos_.set_control_register(*cycle.value);
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
//							printf("TODO: audio adjust %04x <- %02x\n", address, *cycle.value);
						break;

						case 0xb4:
							interrupt_mask_ = *cycle.value & 0x55;
							interrupt_state_ &= ~*cycle.value;
							update_interrupts();
						break;
						case 0xb5:
							active_key_line_ = *cycle.value & 0xf;
							// TODO: printer strobe, type sound, REM switches.
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
		// MARK: - Memory layout
		std::array<uint8_t, 128 * 1024> ram_;
		std::array<uint8_t, 64 * 1024> exos_;
		std::array<uint8_t, 16 * 1024> basic_;
		std::array<uint8_t, 32 * 1024> dos_;
		const uint8_t min_ram_slot_ = uint8_t(0x100 - (ram_.size() / 0x4000));

		const uint8_t *read_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
		uint8_t *write_pointers_[4] = {nullptr, nullptr, nullptr, nullptr};
		uint8_t pages_[4] = {0x80, 0x80, 0x80, 0x80};

		template <size_t slot> void page(uint8_t offset) {
			pages_[slot] = offset;

			if(offset < exos_.size() / 0x4000) {
				page<slot>(&exos_[offset * 0x4000], nullptr);
				return;
			}

			if(offset >= 16 && offset < 16 + basic_.size() / 0x4000) {
				page<slot>(&basic_[(offset - 16) * 0x4000], nullptr);
				return;
			}

			if(offset >= 32 && offset < 32 + dos_.size() / 0x4000) {
				page<slot>(&dos_[(offset - 32) * 0x4000], nullptr);
				return;
			}

			// Of whatever size of RAM I've declared above, use only the final portion.
			// This correlated with Nick always having been handed the final 64kb and,
			// at least while the RAM is the first thing declared above, does a little
			// to benefit data locality. Albeit not in a useful sense.
			if(offset >= min_ram_slot_) {
				const auto ram_floor = 4194304 - ram_.size();
				const size_t address = offset * 0x4000 - ram_floor;
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
			nick_.last_valid()->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return nick_.last_valid()->get_scaled_scan_status();
		}

		// MARK: - TimedMachine
		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		// MARK: - KeyboardMachine
		Enterprise::KeyboardMapper keyboard_mapper_;
		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		uint8_t active_key_line_ = 0;
		std::array<uint8_t, 10> key_lines_;
		void set_key_state(uint16_t key, bool is_pressed) final {
			if(is_pressed) {
				key_lines_[key >> 8] &= ~uint8_t(key);
			} else {
				key_lines_[key >> 8] |= uint8_t(key);
			}
		}

		void clear_all_keys() final {
			key_lines_.fill(0xff);
		}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &media) final {
			if constexpr (has_disk_controller) {
				if(!media.disks.empty()) {
					exdos_.set_disk(media.disks.front(), 0);
				}
			}

			return true;
		}

		// MARK: - Interrupts
		enum class Interrupt: uint8_t {
			Nick = 0x20
		};

		uint8_t interrupt_mask_ = 0x00, interrupt_state_ = 0x00;
		void set_interrupt(Interrupt mask, HalfCycles offset = HalfCycles(0)) {
			interrupt_state_ |= uint8_t(mask);
			update_interrupts(offset);
		}
		void update_interrupts(HalfCycles offset = HalfCycles(0)) {
			z80_.set_interrupt_line((interrupt_state_ >> 1) & interrupt_mask_, offset);
		}

		// MARK: - Chips.
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		JustInTimeActor<Nick, HalfCycles, 40434603, 11360000> nick_;
		bool previous_nick_interrupt_line_ = false;
		// Cf. timing guesses above.

		// MARK: - EXDos card.
		EXDos exdos_;
};

}

using namespace Enterprise;

Machine *Machine::Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target *const enterprise_target = dynamic_cast<const Target *>(target);

	if(enterprise_target->dos == Target::DOS::None)
		return new Enterprise::ConcreteMachine<false>(*enterprise_target, rom_fetcher);
	else
		return new Enterprise::ConcreteMachine<true>(*enterprise_target, rom_fetcher);
}

Machine::~Machine() {}
