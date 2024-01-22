//
//  Amiga.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Amiga.hpp"

#include "../../Activity/Source.hpp"
#include "../MachineTypes.hpp"

#include "../../Processors/68000/68000.hpp"

#include "../../Analyser/Static/Amiga/Target.hpp"

#include "../Utility/MemoryPacker.hpp"
#include "../Utility/MemoryFuzzer.hpp"

#include "../../Outputs/Log.hpp"

#include "Chipset.hpp"
#include "Keyboard.hpp"
#include "MemoryMap.hpp"

#include <cassert>

namespace {

// NTSC clock rate: 2*3.579545 = 7.15909Mhz.
// PAL clock rate: 7.09379Mhz; 227 cycles/line.
constexpr int PALClockRate = 7'093'790;
//constexpr int NTSCClockRate = 7'159'090;

Log::Logger<Log::Source::Amiga> logger;

}

namespace Amiga {

class ConcreteMachine:
	public Activity::Source,
	public CPU::MC68000::BusHandler,
	public MachineTypes::AudioProducer,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::MouseMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Amiga::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this),
			memory_(target.chip_ram, target.fast_ram),
			chipset_(memory_, PALClockRate)
		{
			// Temporary: use a hard-coded Kickstart selection.
			constexpr ROM::Name rom_name = ROM::Name::AmigaA500Kickstart13;
			ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(roms.find(rom_name)->second, memory_.kickstart.data());

			// For now, also hard-code assumption of PAL.
			// (Assumption is both here and in the video timing of the Chipset).
			set_clock_rate(PALClockRate);

			// Insert supplied media.
			insert_media(target.media);
		}

		// MARK: - MediaTarget.

		bool insert_media(const Analyser::Static::Media &media) final {
			return chipset_.insert(media.disks);
		}

		// MARK: - MC68000::BusHandler.
		template <typename Microcycle> HalfCycles perform_bus_operation(const Microcycle &cycle, int) {
			// Do a quick advance check for Chip RAM access; add a suitable delay if required.
			HalfCycles total_length;
			if(cycle.operation & CPU::MC68000::Operation::NewAddress && *cycle.address < 0x20'0000) {
				total_length = chipset_.run_until_after_cpu_slot().duration;
				assert(total_length >= cycle.length);
			} else {
				total_length = cycle.length;
				chipset_.run_for(total_length);
			}
			mc68000_.set_interrupt_level(chipset_.get_interrupt_level());

			// Check for assertion of reset.
			if(cycle.operation & CPU::MC68000::Operation::Reset) {
				memory_.reset();
				logger.info().append("Reset; PC is around %08x", mc68000_.get_state().registers.program_counter);
			}

			// Autovector interrupts.
			if(cycle.operation & CPU::MC68000::Operation::InterruptAcknowledge) {
				mc68000_.set_is_peripheral_address(true);
				return total_length - cycle.length;
			}

			// Do nothing if no address is exposed.
			if(!(cycle.operation & (CPU::MC68000::Operation::NewAddress | CPU::MC68000::Operation::SameAddress))) return total_length - cycle.length;

			// Grab the target address to pick a memory source.
			const uint32_t address = cycle.host_endian_byte_address();

			// Set VPA if this is [going to be] a CIA access.
			mc68000_.set_is_peripheral_address((address & 0xe0'0000) == 0xa0'0000);

			if(!memory_.regions[address >> 18].read_write_mask) {
				if((cycle.operation & (CPU::MC68000::Operation::SelectByte | CPU::MC68000::Operation::SelectWord))) {
					// Check for various potential chip accesses.

					// Per the manual:
					//
					// CIA A is: 101x xxxx xx01 rrrr xxxx xxx0 (i.e. loaded into high byte)
					// CIA B is: 101x xxxx xx10 rrrr xxxx xxx1 (i.e. loaded into low byte)
					//
					// but in order to map 0xbfexxx to CIA A and 0xbfdxxx to CIA B, I think
					// these might be listed the wrong way around.
					//
					// Additional assumption: the relevant CIA select lines are connected
					// directly to the chip enables.
					if((address & 0xe0'0000) == 0xa0'0000) {
						const int reg = address >> 8;
						const bool select_a = !(address & 0x1000);
						const bool select_b = !(address & 0x2000);

						if(cycle.operation & CPU::MC68000::Operation::Read) {
							uint16_t result = 0xffff;
							if(select_a) result &= 0xff00 | (chipset_.cia_a.read(reg) << 0);
							if(select_b) result &= 0x00ff | (chipset_.cia_b.read(reg) << 8);
							cycle.set_value16(result);
						} else {
							if(select_a) chipset_.cia_a.write(reg, cycle.value8_low());
							if(select_b) chipset_.cia_b.write(reg, cycle.value8_high());
						}

//						logger.info().append("CIA %d %s %d of %04x", ((address >> 12) & 3)^3, operation & Microcycle::Read ? "read" : "write", reg & 0xf, cycle.value16());
					} else if(address >= 0xdf'f000 && address <= 0xdf'f1be) {
						chipset_.perform(cycle);
					} else if(address >= 0xe8'0000 && address < 0xe9'0000) {
						// This is the Autoconf space; right now the only
						// Autoconf device this emulator implements is fast RAM,
						// which if present is provided as part of the memory map.
						//
						// Relevant quote: "The Zorro II configuration space is the 64K memory block $00E8xxxx"
						memory_.perform(cycle);
					} else {
						// This'll do for open bus, for now.
						if(cycle.operation & CPU::MC68000::Operation::Read) {
							cycle.set_value16(0xffff);
						}

						// Log only for the region that is definitely not just ROM this machine doesn't have.
						if(address < 0xf0'0000) {
							logger.error().append("Unmapped %s %06x of %04x", cycle.operation & CPU::MC68000::Operation::Read ? "read from " : "write to ", (*cycle.address)&0xffffff, cycle.value16());
						}
					}
				}
			} else {
				// A regular memory access.
				cycle.apply(
					&memory_.regions[address >> 18].contents[address],
					memory_.regions[address >> 18].read_write_mask
				);
			}

			return total_length - cycle.length;
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true, true> mc68000_;

		// MARK: - Memory map.

		MemoryMap memory_;

		// MARK: - Chipset.

		Chipset chipset_;

		// MARK: - Activity Source

		void set_activity_observer(Activity::Observer *observer) final {
			chipset_.set_activity_observer(observer);
		}

		// MARK: - MachineTypes::AudioProducer.

		Outputs::Speaker::Speaker *get_speaker() final {
			return chipset_.get_speaker();
		}

		// MARK: - MachineTypes::ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			chipset_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return chipset_.get_scaled_scan_status();
		}

		// MARK: - MachineTypes::TimedMachine.

		void run_for(const Cycles cycles) final {
			mc68000_.run_for(cycles);
		}

		void flush_output(int) final {
			chipset_.flush();
		}

		// MARK: - MachineTypes::MouseMachine.

		Inputs::Mouse &get_mouse() final {
			return chipset_.get_mouse();;
		}

		// MARK: - MachineTypes::JoystickMachine.

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return chipset_.get_joysticks();
		}

		// MARK: - Keyboard.

		Amiga::KeyboardMapper keyboard_mapper_;
		KeyboardMapper *get_keyboard_mapper() {
			return &keyboard_mapper_;
		}

		void set_key_state(uint16_t key, bool is_pressed) {
			chipset_.get_keyboard().set_key_state(key, is_pressed);
		}

		void clear_all_keys() {
			chipset_.get_keyboard().clear_all_keys();
		}
	};

}


using namespace Amiga;

std::unique_ptr<Machine> Machine::Amiga(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Amiga::Target;
	const Target *const amiga_target = dynamic_cast<const Target *>(target);
	return std::make_unique<Amiga::ConcreteMachine>(*amiga_target, rom_fetcher);
}

Machine::~Machine() {}
