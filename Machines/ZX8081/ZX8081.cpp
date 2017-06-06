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

namespace {
	const unsigned int ZX8081ClockRate = 3250000;
}

Machine::Machine() :
	vsync_(false),
	hsync_(false),
	ram_(1024),
	key_states_{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	tape_player_(ZX8081ClockRate) {
	// run at 3.25 Mhz
	set_clock_rate(ZX8081ClockRate);
	Memory::Fuzz(ram_);
	tape_player_.set_motor_control(true);
}

int Machine::perform_machine_cycle(const CPU::Z80::MachineCycle &cycle) {
	video_->run_for_cycles(cycle.length);
	tape_player_.run_for_cycles(cycle.length);

	uint16_t refresh = 0;
	uint16_t address = cycle.address ? *cycle.address : 0;
	switch(cycle.operation) {
		case CPU::Z80::BusOperation::Output:
//			if((address&7) == 7) {
				set_vsync(false);
				line_counter_ = 0;
//			}
		break;

		case CPU::Z80::BusOperation::Input: {
			uint8_t value = 0xff;
			if(!(address&1)) {
				set_vsync(true);

				uint16_t mask = 0x100;
				for(int c = 0; c < 8; c++) {
					if(!(address & mask)) value &= key_states_[c];
					mask <<= 1;
				}

				value &= ~(tape_player_.get_input() ? 0x80 : 0);
			}
			*cycle.value = value;
		} break;

		case CPU::Z80::BusOperation::Interrupt:
			set_hsync(true);
			line_counter_ = (line_counter_ + 1) & 7;
			*cycle.value = 0xff;
		break;

		case CPU::Z80::BusOperation::ReadOpcode:
			set_hsync(false);
			refresh = get_value_of_register(CPU::Z80::Register::Refresh);
			set_interrupt_line(!(refresh & 0x40));
		case CPU::Z80::BusOperation::Read:
			if((address & 0xc000) == 0x0000) *cycle.value = rom_[address & (rom_.size() - 1)];
			else if((address & 0x4000) == 0x4000) {
				uint8_t value = ram_[address & 1023];
				if(address&0x8000 && !(value & 0x40) && cycle.operation == CPU::Z80::BusOperation::ReadOpcode && !get_halt_line()) {
					size_t char_address = (size_t)((refresh & 0xff00) | ((value & 0x3f) << 3) | line_counter_);
					if((char_address & 0xc000) == 0x0000) {
						uint8_t mask = (value & 0x80) ? 0x00 : 0xff;
						value = rom_[char_address & (rom_.size() - 1)] ^ mask;
					}

					video_->output_byte(value);
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

void Machine::flush() {
	video_->flush();
}

void Machine::setup_output(float aspect_ratio) {
	video_.reset(new Video);
}

void Machine::close_output() {
	video_.reset();
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt() {
	return video_->get_crt();
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker() {
	return nullptr;
}

void Machine::run_for_cycles(int number_of_cycles) {
	CPU::Z80::Processor<Machine>::run_for_cycles(number_of_cycles);
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	// TODO: pay attention to the target; it can't currently specify a ZX81
	// so assume a ZX80 if we got here.
	rom_ = zx80_rom_;

	if(target.tapes.size()) {
		tape_player_.set_tape(target.tapes.front());
	}
}

void Machine::set_rom(ROMType type, std::vector<uint8_t> data) {
	switch(type) {
		case ZX80: zx80_rom_ = data; break;
		case ZX81: zx81_rom_ = data; break;
	}
}

#pragma mark - Video

void Machine::set_vsync(bool sync) {
	vsync_ = sync;
	update_sync();
}

void Machine::set_hsync(bool sync) {
	hsync_ = sync;
	update_sync();
}

void Machine::update_sync() {
	video_->set_sync(vsync_ || hsync_);
}

#pragma mark - Keyboard

void Machine::set_key_state(uint16_t key, bool isPressed) {
	if(isPressed)
		key_states_[key >> 8] &= (uint8_t)(~key);
	else
		key_states_[key >> 8] |= (uint8_t)key;
}

void Machine::clear_all_keys() {
	memset(key_states_, 0xff, 8);
}
