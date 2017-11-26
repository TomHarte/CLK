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

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		ConcreteMachine():
			z80_(*this) {
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

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
