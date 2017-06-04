//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

using namespace ZX8081;

Machine::Machine() {
	// run at 3.25 Mhz
	set_clock_rate(3250000);
}

int Machine::perform_machine_cycle(const CPU::Z80::MachineCycle &cycle) {
	return 0;
}

void Machine::setup_output(float aspect_ratio) {
	crt_.reset(new Outputs::CRT::CRT(207 * 8, 8, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return vec3(1.0);"
		"}");
}

void Machine::flush() {
}

void Machine::close_output() {
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt() {
	return crt_;
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker() {
	return nullptr;
}

void Machine::run_for_cycles(int number_of_cycles) {
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
}

void Machine::set_rom(ROMType type, std::vector<uint8_t> data) {
	switch(type) {
		case ZX80: zx80_rom_ = data; break;
		case ZX81: zx81_rom_ = data; break;
	}
}
