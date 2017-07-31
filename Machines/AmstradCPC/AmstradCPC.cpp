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

	// Stop now if no action is strictly required.
	if(!cycle.is_terminal()) return HalfCycles(0);

	uint16_t address = cycle.address ? *cycle.address : 0x0000;
	switch(cycle.operation) {
		case CPU::Z80::PartialMachineCycle::ReadOpcode:
		case CPU::Z80::PartialMachineCycle::Read:
			switch(address >> 14) {
				case 0: *cycle.value = os_[address & 16383];	break;
				case 1:	case 2:
					*cycle.value = ram_[address];
				break;
				case 3: *cycle.value = basic_[address & 16383];	break;
			}
		break;

		case CPU::Z80::PartialMachineCycle::Write:
			ram_[address] = *cycle.value;
		break;

		case CPU::Z80::PartialMachineCycle::Output:
			// Check for a gate array access.
			if((address & 0xc000) == 0x4000) {
				switch(*cycle.value >> 6) {
					case 0: printf("Select pen %02x\n", *cycle.value & 0x1f);		break;
					case 1: printf("Select colour %02x\n", *cycle.value & 0x1f);	break;
					case 2: printf("Set mode %d, other flags %02x\n", *cycle.value & 3, (*cycle.value >> 2)&7);	break;
					case 3: printf("RAM paging?\n"); break;
				}
			}

			// Check for a CRTC access
			if(!(address & 0x4000)) {
				switch((address >> 8) & 3) {
					case 0:	printf("Select CRTC register %d\n", *cycle.value);	break;
					case 1:	printf("Set CRTC value %d\n", *cycle.value);	break;
					case 2: case 3:	printf("Illegal CRTC write?\n");	break;
				}
			}

			// Check for a PIO access
			if(!(address & 0x800)) {
				switch((address >> 8) & 3) {
					case 0:	printf("PSG data: %d\n", *cycle.value);	break;
					case 1:	printf("Vsync, etc: %02x\n", *cycle.value);	break;
					case 2:	printf("Key row, etc: %02x\n", *cycle.value);	break;
					case 3:	printf("PIO control: %02x\n", *cycle.value);	break;
				}
			}
//			printf("Output %02x -> %04x?\n", *cycle.value, address);
		break;
		case CPU::Z80::PartialMachineCycle::Input:
			printf("Input %04x?\n", address);
		break;

		case CPU::Z80::PartialMachineCycle::Interrupt:
			*cycle.value = 0xff;
		break;

		default: break;
	}

	return HalfCycles(0);
}

void Machine::flush() {
}

void Machine::set_rom(ROMType type, std::vector<uint8_t> data) {
	// Keep only the two ROMs that are currently of interest.
	switch(type) {
		case ROMType::OS464:		os_ = data;		break;
		case ROMType::BASIC464:		basic_ = data;	break;
		default: break;
	}
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
	crt_.reset();
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
