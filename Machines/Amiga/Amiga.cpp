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

#include "../../Components/6526/6526.hpp"

#include "../../Analyser/Static/Amiga/Target.hpp"

#include "../Utility/MemoryPacker.hpp"
#include "../Utility/MemoryFuzzer.hpp"

//#define NDEBUG
#define LOG_PREFIX "[Amiga] "
#include "../../Outputs/Log.hpp"

#include "Chipset.hpp"

namespace Amiga {

class ConcreteMachine:
	public Activity::Source,
	public CPU::MC68000::BusHandler,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Amiga::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this),
			chipset_(reinterpret_cast<uint16_t *>(memory_.chip_ram.data()), memory_.chip_ram.size()),
			cia_a_handler_(memory_),
			cia_a_(cia_a_handler_),
			cia_b_(cia_b_handler_)
		{
			(void)target;

			// Temporary: use a hard-coded Kickstart selection.
			constexpr ROM::Name rom_name = ROM::Name::AmigaA500Kickstart13;
			ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(roms.find(rom_name)->second, memory_.kickstart.data());

			// NTSC clock rate: 2*3.579545 = 7.15909Mhz.
			// PAL clock rate: 7.09379Mhz; 227 cycles/line.
			set_clock_rate(7'093'790.0);
		}

		// MARK: - MC68000::BusHandler.
		using Microcycle = CPU::MC68000::Microcycle;
		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int) {
			// Intended 1-2 step here is:
			//
			//	(1) determine when this CPU access will be scheduled;
			//	(2)	do all the other actions prior to this CPU access being scheduled.
			//
			// (or at least enqueue them, JIT-wise).

			// Advance time.

			// The CIAs are on the E clock.
			// TODO: so they probably should be behind VPA?
			cia_divider_ += cycle.length;
			const HalfCycles e_clocks = cia_divider_.divide(HalfCycles(20));
			if(e_clocks > HalfCycles(0)) {
				cia_a_.run_for(e_clocks);
				cia_b_.run_for(e_clocks);
			}

			const auto changes = chipset_.run_for(cycle.length, false);
			cia_a_.advance_tod(changes.vsyncs);
			cia_b_.advance_tod(changes.hsyncs);
			mc68000_.set_interrupt_level(changes.interrupt_level);

			// Check for assertion of reset.
			if(cycle.operation & Microcycle::Reset) {
				memory_.reset();
				LOG("Reset; PC is around " << PADHEX(8) << mc68000_.get_state().program_counter);
			}

			// Autovector interrupts.
			if(cycle.operation & Microcycle::InterruptAcknowledge) {
				mc68000_.set_is_peripheral_address(true);
			} else {
				mc68000_.set_is_peripheral_address(false);
			}

			// Do nothing if no address is exposed.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// TODO: interrupt acknowledgement.

			// Grab the target address to pick a memory source.
			const uint32_t address = cycle.host_endian_byte_address();
//			if((cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord)) && !(cycle.operation & Microcycle::IsProgram)) {
//				printf("%06x\n", *cycle.address);
//			}

			if(!memory_.regions[address >> 18].read_write_mask) {
				if((cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord))) {
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
						LOG("CIA " << (cycle.operation & Microcycle::Read ? "read " : "write ") << PADHEX(4) << *cycle.address);

						if(cycle.operation & Microcycle::Read) {
							uint16_t result = 0xffff;
							if(!(address & 0x1000)) result &= 0xff00 | (cia_a_.read(reg) << 0);
							if(!(address & 0x2000)) result &= 0x00ff | (cia_b_.read(reg) << 8);
							cycle.set_value16(result);
						} else {
							if(!(address & 0x1000)) cia_a_.write(reg, cycle.value8_low());
							if(!(address & 0x2000)) cia_b_.write(reg, cycle.value8_high());
						}
					} else if(address >= 0xdf'f000 && address <= 0xdf'f1be) {
						chipset_.perform(cycle);
					} else {
						// This'll do for open bus, for now.
						if(cycle.operation & Microcycle::Read) {
							cycle.set_value16(0xffff);
						}

						// Don't log for the region that is definitely just ROM this machine doesn't have.
						if(address < 0xf0'0000) {
							LOG("Unmapped " << (cycle.operation & Microcycle::Read ? "read from " : "write to ") << PADHEX(4) << *cycle.address << " of " << cycle.value16());
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

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;

		// MARK: - Memory map.
		struct MemoryMap {
			public:
				std::array<uint8_t, 512*1024> chip_ram{};
				std::array<uint8_t, 512*1024> kickstart{0xff};

				struct MemoryRegion {
					uint8_t *contents = nullptr;
					unsigned int read_write_mask = 0;
				} regions[64];	// i.e. top six bits are used as an index.

				MemoryMap() {
					// Address spaces that matter:
					//
					//	00'0000 – 08'0000:	chip RAM.	[or overlayed KickStart]
					//	– 10'0000: extended chip ram for ECS.
					//	– 20'0000: auto-config space (/fast RAM).
					//	...
					//	bf'd000 – c0'0000: 8250s.
					//	c0'0000 – d8'0000: pseudo-fast RAM.
					//	...
					//	dc'0000 – dd'0000: optional real-time clock.
					//	df'f000 - e0'0000: custom chip registers.
					//	...
					//	f0'0000 — : 512kb Kickstart (or possibly just an extra 512kb reserved for hypothetical 1mb Kickstart?).
					//	f8'0000 — : 256kb Kickstart if 2.04 or higher.
					//	fc'0000 – : 256kb Kickstart otherwise.
					set_region(0xfc'0000, 0x1'00'0000, kickstart.data(), CPU::MC68000::Microcycle::PermitRead);
					reset();
				}

				void reset() {
					set_overlay(true);
				}

				void set_overlay(bool enabled) {
					if(overlay_ == enabled) {
						return;
					}
					overlay_ = enabled;

					if(enabled) {
						set_region(0x00'0000, 0x08'0000, kickstart.data(), CPU::MC68000::Microcycle::PermitRead);
					} else {
						// Mirror RAM to fill out the address range up to $20'0000 (?)
						set_region(0x00'0000, 0x08'0000, chip_ram.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
					}
				}

			private:
				bool overlay_ = false;

				void set_region(int start, int end, uint8_t *base, unsigned int read_write_mask) {
					assert(!(start & ~0xfc'0000));
					assert(!((end - (1 << 18)) & ~0xfc'0000));

					base -= start;
					for(int c = start >> 18; c < end >> 18; c++) {
						regions[c].contents = base;
						regions[c].read_write_mask = read_write_mask;
					}
				}
		} memory_;

		// MARK: - Chipset.

		Chipset chipset_;

		// MARK: - CIAs.

		class CIAAHandler: public MOS::MOS6526::PortHandler {
			public:
				CIAAHandler(MemoryMap &map) : map_(map) {}

				void set_port_output(MOS::MOS6526::Port port, uint8_t value) {
					if(port) {
						// Parallel port output.
						LOG("TODO: parallel output " << PADHEX(2) << +value);
					} else {
						//	b7:	/FIR1
						//	b6:	/FIR0
						//	b5:	/RDY
						//	b4:	/TRK0
						//	b3:	/WPRO
						//	b2:	/CHNG
						//	b1:	/LED		[output]
						//	b0:	OVL			[output]

						LOG("LED & memory map: " << PADHEX(2) << +value);
						if(observer_) {
							observer_->set_led_status(led_name, !(value & 2));
						}
						map_.set_overlay(value & 1);
					}
				}

				uint8_t get_port_input(MOS::MOS6526::Port port) {
					if(port) {
						LOG("TODO: parallel input?");
					} else {
						LOG("TODO: CIA A, port A input — FIR, RDY, TRK0, etc");

						// Announce that TRK0 is upon us.
						return 0xef;
					}
					return 0xff;
				}

				void set_activity_observer(Activity::Observer *observer) {
					observer_ = observer;
					if(observer) {
						observer->register_led(led_name, Activity::Observer::LEDPresentation::Persistent);
					}
				}

			private:
				MemoryMap &map_;
				Activity::Observer *observer_ = nullptr;
				inline static const std::string led_name = "Power";
		} cia_a_handler_;

		struct CIABHandler: public MOS::MOS6526::PortHandler {
			void set_port_output(MOS::MOS6526::Port port, uint8_t value) {
				if(port) {
					// Serial port control.
					//
					// b7: /DTR
					// b6: /RTS
					// b5: /CD
					// b4: /CTS
					// b3: /DSR
					// b2: SEL
					// b1: POUT
					// b0: BUSY
					LOG("TODO: Serial control: " << PADHEX(2) << +value);
				} else {
					// Disk motor control, drive and head selection,
					// and stepper control:
					//
					// b7: /MTR
					// b6: /SEL3
					// b5: /SEL2
					// b4: /SEL1
					// b3: /SEL0
					// b2: /SIDE
					// b1: DIR
					// b0: /STEP
					LOG("TODO: Stepping, etc; " << PADHEX(2) << +value);
				}
			}
		} cia_b_handler_;

		HalfCycles cia_divider_;
		MOS::MOS6526::MOS6526<CIAAHandler, MOS::MOS6526::Personality::P8250> cia_a_;
		MOS::MOS6526::MOS6526<CIABHandler, MOS::MOS6526::Personality::P8250> cia_b_;

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) final {
			cia_a_handler_.set_activity_observer(observer);
		}

		// MARK: - MachineTypes::ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			chipset_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return chipset_.get_scaled_scan_status();
		}

		// MARK: - MachineTypes::TimedMachine.

		void run_for(const Cycles cycles) {
			mc68000_.run_for(cycles);
		}
};

}


using namespace Amiga;

Machine *Machine::Amiga(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Amiga::Target;
	const Target *const amiga_target = dynamic_cast<const Target *>(target);
	return new Amiga::ConcreteMachine(*amiga_target, rom_fetcher);
}

Machine::~Machine() {}
