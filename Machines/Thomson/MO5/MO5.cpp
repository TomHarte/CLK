//
//  MO5.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MO5.hpp"

#include "Video.hpp"

#include "Machines/MachineTypes.hpp"
#include "Processors/6809/6809.hpp"
#include "Components/6821/6821.hpp"
#include "ClockReceiver/JustInTime.hpp"


using namespace Thomson::MO5;

namespace {

struct ConcreteMachine:
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public Machine
{
	ConcreteMachine(const Analyser::Static::Target &, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6809_(*this),
		system_pia_port_handler_(*this),
		system_pia_(system_pia_port_handler_),
		video_(video_page(true), video_page(false))
	{
		set_clock_rate(1'000'000);

		const auto request = ROM::Request(ROM::Name::ThomasonMO5v11);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto &rom = roms.find(ROM::Name::ThomasonMO5v11)->second;
		std::copy_n(rom.begin(), rom.size(), rom_.begin());

		system_pia_.refresh();
	}

	template <
		int address,
		CPU::M6809::ReadWrite read_write,
		typename ComponentT
	>
	static void access(ComponentT &component, CPU::M6809::data_t<read_write> value) {
		if constexpr (CPU::M6809::is_read(read_write)) {
			value = component.template read<address>();
		} else {
			component.template write<address>(value);
		}
	}

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		if(video_ += m6809_.duration(bus_phase)) {
			system_pia_.set<Motorola::MC6821::Control::CB1>(video_.last_valid()->irq());
		}

		if constexpr (read_write == CPU::M6809::ReadWrite::NoData) {
			return Cycles(0);
		} else {
			if(address >= 0xa7c0 && address < 0xa800) {
				switch(address) {
					case 0xa7c0:	access<0xa7c0, read_write>(system_pia_, value);		break;
					case 0xa7c1:	access<0xa7c1, read_write>(system_pia_, value);		break;
					case 0xa7c2:	access<0xa7c2, read_write>(system_pia_, value);		break;
					case 0xa7c3:	access<0xa7c3, read_write>(system_pia_, value);		break;
					default:
						if constexpr (CPU::M6809::is_read(read_write)) {
							value = 0xff;
							printf("Unhandled read at %04x\n", +address);
						} else {
							printf("Unhandled write: %02x -> %04x\n", +value, +address);
						}
					break;
				}
			} else {
				if constexpr (CPU::M6809::is_read(read_write)) {
					if(address < 0x2000) {
						value = start_pointer_[address];
					} else if(address >= 0xc000) {
						value = rom_[address - 0xc000];
					} else {
						value = ram_[address];
					}
				} else {
					if(address < 0x2000) {
						if(address < 40*200) video_.flush();
						start_pointer_[address] = value;
					} else {
						ram_[address] = value;
					}
				}
			}
		}

		return Cycles(0);
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;

	std::array<uint8_t, 0x10000 + 0x2000> ram_;
	std::array<uint8_t, 0x4000> rom_;
	uint8_t *start_pointer_ = nullptr;

	uint8_t *video_page(const bool pixels) {
		return &ram_[pixels ? 0 : 0x1'0000];
	}

	void page_lower(const bool pixels) {
		start_pointer_ = video_page(pixels);
	}

	friend struct SystemPIAPortHandler;
	struct SystemPIAPortHandler {
		SystemPIAPortHandler(ConcreteMachine &machine) : machine_(machine) {}

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				//	Port A inputs:
				//		b4: light pen button
				//		b7: tape input [and 0 = no tape; 1 = tape present]
				return 0xff;
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				//	Port B inputs:
				//		b7: status of key at that position.
				return 0xff;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			if constexpr (port == Motorola::MC6821::Port::A) {
				machine_.page_lower(value & 1);
				machine_.video_->set_border_colour((value >> 1) & 0xf);

				//	Port A outputs:
				//		b0 = lower 8kb RAM paging;
				//		b1–4: border colour;
				//		b6: tape output
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
//				printf("Keyboard scan: %d\n", (value >> 1) & 0x3f);
				// Port B outputs:
				//		b0 = 1-bit sound output;
				//		b1–3 = keyboard column;
				//		b4–6: keyboard line;
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool active) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
				machine_.m6809_.set<CPU::M6809::Line::FIRQ>(active);
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
				machine_.m6809_.set<CPU::M6809::Line::IRQ>(active);
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool value) {
			// TODO: CA2 is drive motor control, so catch that.
			(void)value;
		}

		// TODO:
		//
		//	CA1: lightpen input
		//	CA2: drive motor control
		//	CB1: 50Hz interrupt
		//	CB2: genlock enable, maybe? Video "encrustation".

	private:
		ConcreteMachine &machine_;

	};
	SystemPIAPortHandler system_pia_port_handler_;
	Motorola::MC6821::MC6821<SystemPIAPortHandler, 2, 1> system_pia_;

	JustInTimeActor<Video, Cycles> video_;

	// MARK: - ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		video_->set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return video_.last_valid()->get_scaled_scan_status();
	}

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}
	void flush_output(int outputs) final {
		if(outputs & Output::Video) {
			video_.flush();
		}
	}
};

}

std::unique_ptr<Machine> Machine::ThomsonMO(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
