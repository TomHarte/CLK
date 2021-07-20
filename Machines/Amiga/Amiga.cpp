//
//  Amiga.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Amiga.hpp"

#include "../MachineTypes.hpp"

#include "../../Processors/68000/68000.hpp"

#include "../../Components/6526/6526.hpp"

#include "../../Analyser/Static/Amiga/Target.hpp"

#include "../Utility/MemoryPacker.hpp"
#include "../Utility/MemoryFuzzer.hpp"

#define LOG_PREFIX "[Amiga] "
#include "../../Outputs/Log.hpp"

namespace Amiga {

class ConcreteMachine:
	public CPU::MC68000::BusHandler,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::Amiga::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this),
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
			Memory::PackBigEndian16(roms.find(rom_name)->second, memory_.kickstart_.data());

			// NTSC clock rate: 2*3.579545 = 7.15909Mhz.
			// PAL clock rate: 7.09379Mhz.
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

			// TODO: I think there's a divide-by-ten here. Probably these are driven off the 68000 E clock?
			cia_a_.run_for(cycle.length);
			cia_b_.run_for(cycle.length);

			// Check for assertion of reset.
			if(cycle.operation & Microcycle::Reset) {
				LOG("Unhandled Reset; PC is around " << PADHEX(8) << mc68000_.get_state().program_counter);
			}

			// Do nothing if no address is exposed.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// TODO: interrupt acknowledgement.

			// Grab the target address to pick a memory source.
			const uint32_t address = cycle.host_endian_byte_address();
//			if(cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord)) {
//				printf("%06x\n", *cycle.address);
//			}

			if(!memory_.regions_[address >> 18].read_write_mask) {
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
						LOG("CIA access: " << PADHEX(4) << *cycle.address);

						if(cycle.operation & Microcycle::Read) {
							uint16_t result = 0xffff;
							if(!(address & 0x1000)) result &= 0x00ff | (cia_a_.read(reg) << 8);
							if(!(address & 0x2000)) result &= 0xff00 | (cia_b_.read(reg) << 0);
							cycle.set_value16(result);
						} else {
							if(!(address & 0x1000)) cia_a_.write(reg, cycle.value8_high());
							if(!(address & 0x2000)) cia_b_.write(reg, cycle.value8_low());
						}
					} else if(address >= 0xdf'f000 && address <= 0xdf'f1be) {
#define RW(address)		(address & 0xffe) | ((cycle.operation & Microcycle::Read) << 7)
#define Read(address)	address | 0x1000
#define Write(address)	address

#define ApplySetClear(target)	{			\
	const uint16_t value = cycle.value16();	\
	if(value & 0x8000) {					\
		target |= (value & 0x7fff);			\
	} else {								\
		target &= ~(value & 0x7fff);		\
	}										\
}

						switch(RW(address)) {
							default:
								printf("Unimplemented chipset access %06x\n", *cycle.address);
								assert(false);
							break;

							// Disk DMA.
							case Write(0x020):	case Write(0x022):	case Write(0x024):
							case Write(0x026):
								LOG("TODO: disk DMA; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;

							// Refresh.
							case Write(0x028):
								LOG("TODO (maybe): refresh; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;

							// Serial port.
							case Write(0x030):
								LOG("TODO: serial data: " << PADHEX(4) << cycle.value16());
							break;
							case Write(0x032):
								LOG("TODO: serial control: " << PADHEX(4) << cycle.value16());
							break;

							// DMA management.
							case Write(0x096):
								ApplySetClear(dma_control_);
								LOG("DMA control modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{dma_control_});
							break;

							// Interrupts.
							case Write(0x09a):
								ApplySetClear(interrupt_enable_);
								update_interrupts();
								LOG("Interrupt enable mask modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{interrupt_enable_});
							break;
							case Write(0x09c):
								ApplySetClear(interrupt_requests_);
								update_interrupts();
								LOG("Interrupt request modified by " << PADHEX(4) << cycle.value16() << "; is now " << std::bitset<16>{interrupt_requests_});
							break;

							// Bitplanes.
							case Write(0x100):
							case Write(0x102):
							case Write(0x104):
							case Write(0x106):
								LOG("TODO: Bitplane control; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;

							case Write(0x108):
							case Write(0x10a):
								LOG("TODO: Bitplane modulo; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;

							case Write(0x110):
							case Write(0x112):
							case Write(0x114):
							case Write(0x116):
							case Write(0x118):
							case Write(0x11a):
								LOG("TODO: Bitplane data; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;

							// Colour palette.
							case Write(0x180):	case Write(0x182):	case Write(0x184):	case Write(0x186):
							case Write(0x188):	case Write(0x18a):	case Write(0x18c):	case Write(0x18e):
							case Write(0x190):	case Write(0x192):	case Write(0x194):	case Write(0x196):
							case Write(0x198):	case Write(0x19a):	case Write(0x19c):	case Write(0x19e):
							case Write(0x1a0):	case Write(0x1a2):	case Write(0x1a4):	case Write(0x1a6):
							case Write(0x1a8):	case Write(0x1aa):	case Write(0x1ac):	case Write(0x1ae):
							case Write(0x1b0):	case Write(0x1b2):	case Write(0x1b4):	case Write(0x1b6):
							case Write(0x1b8):	case Write(0x1ba):	case Write(0x1bc):	case Write(0x1be):
								LOG("TODO: colour palette; " << PADHEX(4) << cycle.value16() << " to " << *cycle.address);
							break;
						}

#undef ApplySetClear

#undef Write
#undef Read
#undef RW
					} else {
						// This'll do for open bus, for now.
						cycle.set_value16(0xffff);
						LOG("Unmapped access to " << PADHEX(4) << *cycle.address);
					}
				}
			} else {
				// A regular memory access.
				cycle.apply(
					&memory_.regions_[address >> 18].contents[address],
					memory_.regions_[address >> 18].read_write_mask
				);
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;

		// MARK: - Memory map.
		struct MemoryMap {
			public:
				std::array<uint8_t, 512*1024> ram_;
				std::array<uint8_t, 512*1024> kickstart_;

				struct MemoryRegion {
					uint8_t *contents = nullptr;
					unsigned int read_write_mask = 0;
				} regions_[64];	// i.e. top six bits are used as an index.

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
					set_region(0xfc'0000, 0x1'00'0000, kickstart_.data(), CPU::MC68000::Microcycle::PermitRead);
					set_overlay(true);
				}

				void set_overlay(bool enabled) {
					if(overlay_ == enabled) {
						return;
					}
					overlay_ = enabled;

					if(enabled) {
						set_region(0x00'0000, 0x08'00000, kickstart_.data(), CPU::MC68000::Microcycle::PermitRead);
					} else {
						set_region(0x00'0000, 0x08'00000, ram_.data(), CPU::MC68000::Microcycle::PermitRead | CPU::MC68000::Microcycle::PermitWrite);
					}
				}

			private:
				bool overlay_ = false;

				void set_region(int start, int end, uint8_t *base, unsigned int read_write_mask) {
					assert(!(start & ~0xfc'0000));
					assert(!((end - (1 << 18)) & ~0xfc'0000));

					for(int c = start >> 18; c < end >> 18; c++) {
						regions_[c].contents = base - (c << 18);
						regions_[c].read_write_mask = read_write_mask;
					}
				}
		} memory_;

		// MARK: - Interrupts.

		uint16_t interrupt_enable_ = 0;
		uint16_t interrupt_requests_ = 0;

		void update_interrupts() {
			// TODO.
		}

		// MARK: - DMA control.

		uint16_t dma_control_ = 0;

		// MARK: - CIAs.

		class CIAAHandler: public MOS::MOS6526::PortHandler {
			public:
				CIAAHandler(MemoryMap &map) : map_(map) {}

				void set_port_output(MOS::MOS6526::Port port, uint8_t value) {
					if(port) {
						// Parallel port output.
						LOG("TODO: parallel output " << PADHEX(2) << value);
					} else {
						//	b7:	/FIR1
						//	b6:	/FIR0
						//	b5:	/RDY
						//	b4:	/TRK0
						//	b3:	/WPRO
						//	b2:	/CHNG
						//	b1:	/LED		[output]
						//	b0:	OVL			[output]

						LOG("TODO: LED -> " << (value & 2));
						map_.set_overlay(value & 1);
					}
				}

				uint8_t get_port_input(MOS::MOS6526::Port port) {
					(void)port;
					return 0xff;
				}

			private:
				MemoryMap &map_;
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
					LOG("TODO: Serial control: " << PADHEX(2) << value);
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
					LOG("TODO: Stepping, etc; " << PADHEX(2) << value);
				}
			}
		} cia_b_handler_;

		MOS::MOS6526::MOS6526<CIAAHandler, MOS::MOS6526::Personality::P8250> cia_a_;
		MOS::MOS6526::MOS6526<CIABHandler, MOS::MOS6526::Personality::P8250> cia_b_;

		// MARK: - MachineTypes::ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return Outputs::Display::ScanStatus();
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
