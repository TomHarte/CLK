//
//  ColecoVision.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ColecoVision.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"

#include "../CRTMachine.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

namespace Coleco {
namespace Vision {

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine {

	public:
		ConcreteMachine() : z80_(*this) {
			set_clock_rate(3579545);
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS9918(TI::TMS9918::TMS9918A));
			get_crt()->set_output_device(Outputs::CRT::OutputDevice::Television);
		}

		void close_output() override {
			vdp_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return vdp_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"ColecoVision",
				{
					"coleco.rom"
				});

			if(!roms[0]) return false;

			bios_ = *roms[0];
			bios_.resize(8192);

			return true;
		}

		// MARK: Z80::BusHandler
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_non_maskable_interrupt_line(true, time_until_interrupt_);
				}
			}

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					if(address < 0x2000) {
						*cycle.value = bios_[address];
					} else if(address >= 0x6000 && address < 0x8000) {
						*cycle.value = ram_[address & 1023];
					} else {
						*cycle.value = 0xff;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(address >= 0x6000 && address < 0x8000) {
						ram_[address & 1023] = *cycle.value;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch((address >> 5) & 7) {
						case 5:
							vdp_->run_for(time_since_vdp_update_.flush());
							*cycle.value = vdp_->get_register(address);
							z80_.set_non_maskable_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						default:
							*cycle.value = 0xff;
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					switch((address >> 5) & 7) {
						case 5:
							vdp_->run_for(time_since_vdp_update_.flush());
							vdp_->set_register(address, *cycle.value);
							z80_.set_non_maskable_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						default: break;
					}
				break;

				default:
				break;
			}

			time_since_vdp_update_ += cycle.length;
			return HalfCycles(0);
		}

		void flush() {
			vdp_->run_for(time_since_vdp_update_.flush());
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;

		std::vector<uint8_t> bios_;
		uint8_t ram_[1024];

		HalfCycles time_since_vdp_update_;
		HalfCycles time_until_interrupt_;
};

}
}

using namespace Coleco::Vision;

Machine *Machine::ColecoVision() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
