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
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../CRTMachine.hpp"

namespace MSX {

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine {
	public:
		ConcreteMachine():
			z80_(*this) {
			set_clock_rate(3579545);
		}

		void setup_output(float aspect_ratio) override {
		}

		void close_output() override {
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override {
			return nullptr;
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
