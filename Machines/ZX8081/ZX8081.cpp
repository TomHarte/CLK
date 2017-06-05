//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

#include "../MemoryFuzzer.hpp"

using namespace ZX8081;

Machine::Machine() :
	vsync_(false),
	hsync_(false),
	ram_(1024) {
	// run at 3.25 Mhz
	set_clock_rate(3250000);
	Memory::Fuzz(ram_);
}

int Machine::perform_machine_cycle(const CPU::Z80::MachineCycle &cycle) {
	cycles_since_display_update_ += cycle.length;

	uint8_t r;
	uint16_t address = cycle.address ? *cycle.address : 0;
	switch(cycle.operation) {
		case CPU::Z80::BusOperation::Output:
			if((address&7) == 7) {
				set_vsync(false);
			}
		break;

		case CPU::Z80::BusOperation::Input:
			if((address&7) == 6) {
				set_vsync(true);
			}
			*cycle.value = 0xff;
		break;

		case CPU::Z80::BusOperation::Interrupt:
			set_hsync(true);
			*cycle.value = 0xff;
		break;

		case CPU::Z80::BusOperation::ReadOpcode:
			set_hsync(false);
			r = (uint8_t)get_value_of_register(CPU::Z80::Register::R);
			set_interrupt_line(!(r & 0x40));
		case CPU::Z80::BusOperation::Read:
			if((address & 0xc000) == 0x0000) *cycle.value = rom_[address & (rom_.size() - 1)];
			else if((address & 0x4000) == 0x4000) {
				uint8_t value = ram_[address & 1023];
				if(address&0x8000 && !(value & 0x40) && cycle.operation == CPU::Z80::BusOperation::ReadOpcode && !get_halt_line()) {
					// TODO: character lookup.
					output_byte(value);
					*cycle.value = 0;
				} else *cycle.value = value;
			}
		break;

		case CPU::Z80::BusOperation::Write:
			if((address & 0x4000) == 0x4000) ram_[address & 1023] = *cycle.value;
		break;

		default: break;
	}

	return 0;
}

void Machine::setup_output(float aspect_ratio) {
	crt_.reset(new Outputs::CRT::CRT(207, 1, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return vec3(1.0);"
		"}");
}

void Machine::flush() {
	update_display();
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
	CPU::Z80::Processor<Machine>::run_for_cycles(number_of_cycles);
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	// TODO: pay attention to the target
	rom_ = zx80_rom_;
}

void Machine::set_rom(ROMType type, std::vector<uint8_t> data) {
	switch(type) {
		case ZX80: zx80_rom_ = data; break;
		case ZX81: zx81_rom_ = data; break;
	}
}

#pragma mark - Video

void Machine::update_display() {
	// TODO.
	cycles_since_display_update_ = 0;
}

void Machine::set_vsync(bool sync) {
	if(sync == vsync_) return;
	vsync_ = sync;
}

void Machine::set_hsync(bool sync) {
	if(sync == hsync_) return;
	hsync_ = sync;
}

void Machine::output_byte(uint8_t byte) {
}
