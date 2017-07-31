//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

using namespace AmstradCPC;

Machine::Machine() {
	// primary clock is 4Mhz
	set_clock_rate(4000000);
}

HalfCycles Machine::perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
	// Amstrad CPC timing scheme: assert WAIT for three out of four cycles
	clock_offset_ = (clock_offset_ + cycle.length) & HalfCycles(7);
	set_wait_line(clock_offset_ >= HalfCycles(2));

	return HalfCycles(0);
}

void Machine::flush() {
}

void Machine::setup_output(float aspect_ratio) {
	crt_.reset(new Outputs::CRT::CRT(256, 1, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return vec3(1.0);"
		"}");
}

void Machine::close_output() {
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt() {
	return crt_;
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker() {
	return nullptr;
}

void Machine::run_for(const Cycles cycles) {
	CPU::Z80::Processor<Machine>::run_for(cycles);
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
}
