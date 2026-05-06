//
//  CoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCo.hpp"

#include "Processors/6809/6809.hpp"
#include "Components/6821/6821.hpp"
#include "Components/6847/6847.hpp"

#include "Machines/MachineTypes.hpp"
#include "Analyser/Static/TandyCoCo/Target.hpp"

using namespace Tandy::CoCo;

namespace {

struct MemoryMap {
	uint8_t read(const uint16_t address) const {
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
		m6809_(*this),
		m6847_(ram_),
		pia0_(pia0_handler_),
		pia1_handler_(*this),
		pia1_(pia1_handler_)
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
		// TODO, maybe: pull the switch inside this SAM call outside the loop?
		const auto delay = sam_.cycle_cost<bus_phase, read_write>(address);
		const auto duration = delay + CPU::M6809::duration<Cycles>(bus_phase);
		m6847_.run_for(duration * 4);

		using namespace CPU::M6809;
		if(address >> 8 == 0xff) {
			if constexpr (read_write != ReadWrite::NoData) {
				switch(address) {
					default: printf("Unhandled at %04x\n", +address);	break;

					case 0xff00:	case 0xff04:	case 0xff08:	case 0xff0c:
					case 0xff10:	case 0xff14:	case 0xff18:	case 0xff1c:
						access<0xff00, read_write>(pia0_, value);
					break;
					case 0xff01:	case 0xff05:	case 0xff09:	case 0xff0d:
					case 0xff11:	case 0xff15:	case 0xff19:	case 0xff1d:
						access<0xff01, read_write>(pia0_, value);
					break;
					case 0xff02:	case 0xff06:	case 0xff0a:	case 0xff0e:
					case 0xff12:	case 0xff16:	case 0xff1a:	case 0xff1e:
						access<0xff02, read_write>(pia0_, value);
					break;
					case 0xff03:	case 0xff07:	case 0xff0b:	case 0xff0f:
					case 0xff13:	case 0xff17:	case 0xff1b:	case 0xff1f:
						access<0xff03, read_write>(pia0_, value);
					break;

					case 0xff20:	case 0xff24:	case 0xff28:	case 0xff2c:
					case 0xff30:	case 0xff34:	case 0xff38:	case 0xff3c:
						access<0xff20, read_write>(pia1_, value);
					break;
					case 0xff21:	case 0xff25:	case 0xff29:	case 0xff2d:
					case 0xff31:	case 0xff35:	case 0xff39:	case 0xff3d:
						access<0xff21, read_write>(pia1_, value);
					break;
					case 0xff22:	case 0xff26:	case 0xff2a:	case 0xff2e:
					case 0xff32:	case 0xff36:	case 0xff3a:	case 0xff3e:
						access<0xff22, read_write>(pia1_, value);
					break;
					case 0xff23:	case 0xff27:	case 0xff2b:	case 0xff2f:
					case 0xff33:	case 0xff37:	case 0xff3b:	case 0xff3f:
						access<0xff23, read_write>(pia1_, value);
					break;

					case 0xffc0:	sam_.access<0xffc0>();	break;		case 0xffc1:	sam_.access<0xffc1>();	break;
					case 0xffc2:	sam_.access<0xffc2>();	break;		case 0xffc3:	sam_.access<0xffc3>();	break;
					case 0xffc4:	sam_.access<0xffc4>();	break;		case 0xffc5:	sam_.access<0xffc5>();	break;
					case 0xffc6:	sam_.access<0xffc6>();	break;		case 0xffc7:	sam_.access<0xffc7>();	break;
					case 0xffc8:	sam_.access<0xffc8>();	break;		case 0xffc9:	sam_.access<0xffc9>();	break;
					case 0xffca:	sam_.access<0xffca>();	break;		case 0xffcb:	sam_.access<0xffcb>();	break;
					case 0xffcc:	sam_.access<0xffcc>();	break;		case 0xffcd:	sam_.access<0xffcd>();	break;
					case 0xffce:	sam_.access<0xffce>();	break;		case 0xffcf:	sam_.access<0xffcf>();	break;
					case 0xffd0:	sam_.access<0xffd0>();	break;		case 0xffd1:	sam_.access<0xffd1>();	break;
					case 0xffd2:	sam_.access<0xffd2>();	break;		case 0xffd3:	sam_.access<0xffd3>();	break;
					case 0xffd4:	sam_.access<0xffd4>();	break;		case 0xffd5:	sam_.access<0xffd5>();	break;
					case 0xffd6:	sam_.access<0xffd6>();	break;		case 0xffd7:	sam_.access<0xffd7>();	break;
					case 0xffd8:	sam_.access<0xffd8>();	break;		case 0xffd9:	sam_.access<0xffd9>();	break;
					case 0xffda:	sam_.access<0xffda>();	break;		case 0xffdb:	sam_.access<0xffdb>();	break;
					case 0xffdc:	sam_.access<0xffdc>();	break;		case 0xffdd:	sam_.access<0xffdd>();	break;
					case 0xffde:	sam_.access<0xffde>();	break;		case 0xffdf:	sam_.access<0xffdf>();	break;

					case 0xfff2:	case 0xfff3:	case 0xfff4:	case 0xfff5:
					case 0xfff6:	case 0xfff7:	case 0xfff8:	case 0xfff9:
					case 0xfffa:	case 0xfffb:	case 0xfffc:	case 0xfffd:
					case 0xfffe:	case 0xffff:
						if constexpr (is_read(read_write)) {
							value = memory_.read(address - 0x4000);
						}
					break;
				}
			}
		} else {
			if constexpr (is_read(read_write)) {
				value = memory_.read(address);
			}

			if constexpr (is_write(read_write)) {
				memory_.write(address, value);
			}
		}

		return duration;
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

	// MARK: - Video and ScanProducer.

	// TODO: video should go through the SAM and its independent video counter.
	Motorola::MC6847::MC6847<
		Motorola::MC6847::FrameTiming::NTSC,
		std::array<uint8_t, 64 * 1024>
	> m6847_;

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		m6847_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return m6847_.get_scaled_scan_status() / 2.0;
	}

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	void flush_output(const int outputs) final {
		(void)outputs;
	}

	// MARK: - PIAs.

	friend struct PIA0Handler;
	struct PIA0Handler {
		//
		// Port A:
		//
		//	b7: joystick comparison input
		//	b0–b6: keyboard row [input?]
		//	b0–b1, b2–b3: joystick button inputs;
		//		0 and 2 = right joystick, 1 and 3 = left joystick;
		//		0 and 1 = switch 1, 2 and 3 = switch 2.
		//
		//	CA1: hsync
		//	CA2: "select line LSB of MUX"?

		//
		// Port B:
		//
		//	b0-b7: keyboard column [output?]
		//
		//	CB1: vsync
		//	CB2: "select lime MSB of MUX"?

		// TODO: what MUX?

		// Interrupt output connected to IRQ.

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				return 0xff;
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				return 0xff;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t) {
			if constexpr (port == Motorola::MC6821::Port::A) {
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool) {
			if constexpr (control == Motorola::MC6821::Control::CA2) {
			}
			if constexpr (control == Motorola::MC6821::Control::CB2) {
			}
		}
	};
	PIA0Handler pia0_handler_;
	Motorola::MC6821::MC6821<PIA0Handler> pia0_;

	friend struct PIA1Handler;
	struct PIA1Handler {
		PIA1Handler(ConcreteMachine &machine) : machine_(machine) {}

		//
		// Port A:
		//
		//	b2-b7: 6-bit DAC output
		//	b1: RS232 data output
		//	b0: tape output
		//
		//	CA1: RS232 carrier detect
		//	CA2: cassette motor control

		//
		// Port B:
		//
		//	b7: 6847 alpha/graphics select (0 = alphanumeric)
		//	b4–b6: VDG GM inputs; also b5 = 6847 invert; b4 = 6847 shift toggle
		//	b3: colour set select (and RGB monitor detecting input? Probably CoCo3)
		//	b2: ram size input
		//	b1: tape input
		//	b0: RS232 data input
		//
		//	CB1: cartridge interrupt input
		//	CB2: sound enable

		// Interrupt output connected to FIRQ.

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				return 0xff;
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				return 0xff;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			if constexpr (port == Motorola::MC6821::Port::A) {
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				machine_.m6847_.set_mode(
					value & 0x80,		// Alpha/graphics.
					value & 0x80,		// Graphics/semigraphics.
					false,				// External ROM.
					value & 0x20,		// Invert.
					(value >> 4) & 7,	// Graphics mode.
					value & 0x80		// Colour select.
				);
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool) {
			if constexpr (control == Motorola::MC6821::Control::CA2) {
			}
			if constexpr (control == Motorola::MC6821::Control::CB2) {
			}
		}

	private:
		ConcreteMachine &machine_;
	};
	PIA1Handler pia1_handler_;
	Motorola::MC6821::MC6821<PIA1Handler> pia1_;

	// MARK: - SAM.

	struct SAM {
		template <uint16_t address> void access() {
			const auto set = [&](auto &target, auto bit) {
				target = target & ~bit;
				target |= address & 1 ? bit : 0;
			};

			switch((address >> 1) & 0xf) {
				default:
					printf("Unhandled SAM access %04x\n", address);
				break;

				case 0:	set(graphics_mode_, 1);	break;
				case 1:	set(graphics_mode_, 2);	break;
				case 2:	set(graphics_mode_, 4);	break;

				case 3:	set(graphics_address_, 0x0200);	break;
				case 4:	set(graphics_address_, 0x0400);	break;
				case 5:	set(graphics_address_, 0x0800);	break;
				case 6:	set(graphics_address_, 0x1000);	break;
				case 7:	set(graphics_address_, 0x2000);	break;
				case 8:	set(graphics_address_, 0x4000);	break;
				case 9:	set(graphics_address_, 0x8000);	break;

				case 10:
					page1_ = address & 1;
					if(page1_) {
						printf("TODO: RAM page 1\n");
						// TODO: communicate to memory map.
					}
				break;

				case 11: { int tc = int(speed_); set(tc, 1); speed_ = ClockSpeed(tc); }	break;
				case 12: { int tc = int(speed_); set(tc, 2); speed_ = ClockSpeed(tc); }	break;

				// Likely these are to effect refresh; TODO: look into this.
				case 13: set(ram_size_, 1);	break;
				case 14: set(ram_size_, 2);	break;

				case 15:
					all_ram_ = address & 1;
					if(all_ram_) {
						printf("TODO: all-RAM mode\n");
						// TODO: communicate to memory map.
					}
				break;
			}
		}

		template <
			CPU::M6809::BusPhase bus_phase,
			CPU::M6809::ReadWrite read_write,
			typename AddressT
		> Cycles cycle_cost(const AddressT address) {
			switch(speed_) {
				case ClockSpeed::Full1:
				case ClockSpeed::Full2:
				return Cycles(0);

				case ClockSpeed::Half:
				return duration<Cycles>(bus_phase);

				case ClockSpeed::HalfInRAM:
					if constexpr (read_write == CPU::M6809::ReadWrite::NoData) {
						return Cycles(0);
					} else {
						if(address < 0x8000 || all_ram_) {
							return duration<Cycles>(bus_phase);
						} else {
							return Cycles(0);
						}
					}
				break;

				default: break;
			}
			__builtin_unreachable();
		}

	private:
		enum class ClockSpeed {
			Half,
			HalfInRAM,
			Full1,
			Full2,
		};

		int graphics_mode_ = 0;
		uint16_t graphics_address_ = 0;
		ClockSpeed speed_ = ClockSpeed::Half;
		bool all_ram_ = false;
		bool page1_ = false;
		int ram_size_ = 0;
	};
	SAM sam_;
};

}

std::unique_ptr<Machine> Machine::create(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const auto &coco_target = static_cast<const Analyser::Static::TandyCoCo::Target &>(target);
	return std::make_unique<TandyCoCo::ConcreteMachine>(coco_target, rom_fetcher);
}
