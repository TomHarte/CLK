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
		pia0_(pia0_handler_),
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

	// MARK: - PIAs.

	friend struct PIA0Handler;
	struct PIA0Handler {
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
	PIA0Handler pia1_handler_;
	Motorola::MC6821::MC6821<PIA0Handler> pia1_;

	// MARK: - SAM.

	struct SAM {
		template <uint16_t> void access() {}
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
