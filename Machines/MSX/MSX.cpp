//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/1770/1770.hpp"
#include "../../Components/9918/9918.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../CRTMachine.hpp"
#include "../ConfigurationTarget.hpp"

namespace MSX {

class i8255PortHandler: public Intel::i8255::PortHandler {
};

class AYPortHandler: public GI::AY38910::PortHandler {
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		ConcreteMachine():
			z80_(*this),
			i8255_(i8255_port_handler_) {
			ay_.set_port_handler(&ay_port_handler_);
			set_clock_rate(3579545);
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS9918(TI::TMS9918::TMS9918A));
		}

		void close_output() override {
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override {
			return vdp_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}
		
		void configure_as_target(const StaticAnalyser::Target &target) override {
		}
		
		bool insert_media(const StaticAnalyser::Media &media) override {
			return true;
		}

		HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					*cycle.value = read_pointers_[address >> 14][address & 16383];
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					write_pointers_[address >> 14][address & 16383] = *cycle.value;
				break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch(address & 0xff) {
						case 0x98:	case 0x99:
							*cycle.value = vdp_->get_register(address);
						break;

						case 0xa2:
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(GI::AY38910::BDIR | GI::AY38910::BC1));
							ay_.set_data_input(*cycle.value);
						break;

						case 0xa8:	case 0xa9:
						case 0xaa:	case 0xab:
							*cycle.value = i8255_.get_register(address);
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					switch(address & 0xff) {
						case 0x98:	case 0x99:
							printf("VDP %d %02x\n", address&1, *cycle.value);
							vdp_->set_register(address, *cycle.value);
						break;

						case 0xa0:
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(GI::AY38910::BDIR | GI::AY38910::BC2 | GI::AY38910::BC1));
							ay_.set_data_input(*cycle.value);
						break;

						case 0xa1:
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(GI::AY38910::BDIR | GI::AY38910::BC2));
							ay_.set_data_input(*cycle.value);
						break;

						case 0xa8:	case 0xa9:
						case 0xaa:	case 0xab:
							printf("8255 %d %02x\n", address&3, *cycle.value);
							i8255_.set_register(address, *cycle.value);
						break;
					}
				break;

				default: break;
			}

			// Per the best information I currently have, the MSX inserts an extra cycle into each opcode read,
			// but otherwise runs without pause.
			return HalfCycles((cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) ? 2 : 0);;
		}

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"MSX",
				{
					"basic.rom",
					"main_msx1.rom"
				});

			if(!roms[0] || !roms[1]) return false;

			basic_ = std::move(*roms[0]);
			basic_.resize(16384);

			main_ = std::move(*roms[1]);
			main_.resize(16384);

			for(size_t c = 0; c < 4; ++c) {
				write_pointers_[c] = &ram_[c * 16384];
				read_pointers_[c] = &ram_[c * 16384];
			}
			read_pointers_[0] = main_.data();
			write_pointers_[0] = scratch_;
			read_pointers_[1] = basic_.data();
			write_pointers_[1] = scratch_;

			return true;
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;
		GI::AY38910::AY38910 ay_;

		i8255PortHandler i8255_port_handler_;
		AYPortHandler ay_port_handler_;

		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];
		uint8_t ram_[65536];
		uint8_t scratch_[16384];
		std::vector<uint8_t> basic_, main_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
