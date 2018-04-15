//
//  AppleII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleII.hpp"

#include "../CRTMachine.hpp"
#include "../Utility/MemoryFuzzer.hpp"

#include "../../Processors/6502/6502.hpp"

#include "Video.hpp"

#include <memory>

namespace {

class ConcreteMachine:
	public CRTMachine::Machine,
	public CPU::MOS6502::BusHandler,
	public AppleII::Machine {
	public:

		ConcreteMachine():
		 	m6502_(*this) {
			set_clock_rate(1022727);
			Memory::Fuzz(ram_, sizeof(ram_));
		}

		void setup_output(float aspect_ratio) override {
			video_.reset(new AppleII::Video);
		}

		void close_output() override {
			video_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return video_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
			m6502_.run_for(cycles);
		}

		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(isReadOperation(operation)) {
				if(address < sizeof(ram_)) {
					*value = ram_[address];
				} else if(address >= rom_start_address_) {
					*value = rom_[address - rom_start_address_];
				}
			} else {
				if(address < sizeof(ram_)) {
					ram_[address] = *value;
				}
			}
			return Cycles(1);
		}

		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"AppleII",
				{
					"apple2o.rom"
				});

			if(!roms[0]) return false;
			rom_ = std::move(*roms[0]);
			rom_start_address_ = static_cast<uint16_t>(0x10000 - rom_.size());

			return true;
		}

	private:
		CPU::MOS6502::Processor<ConcreteMachine, false> m6502_;
		std::unique_ptr<AppleII::Video> video_;

		uint8_t ram_[48*1024];
		std::vector<uint8_t> rom_;
		uint16_t rom_start_address_;
};

}

using namespace AppleII;

Machine *Machine::AppleII() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
