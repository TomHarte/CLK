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
	ram_(1024),
	line_data_(nullptr),
	key_states_{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} {
	// run at 3.25 Mhz
	set_clock_rate(3250000);
	Memory::Fuzz(ram_);
}

int Machine::perform_machine_cycle(const CPU::Z80::MachineCycle &cycle) {
	cycles_since_display_update_ += (unsigned int)cycle.length;

	uint16_t refresh = 0;
	uint16_t address = cycle.address ? *cycle.address : 0;
	switch(cycle.operation) {
		case CPU::Z80::BusOperation::Output:
			if((address&7) == 7) {
				set_vsync(false);
			}
		break;

		case CPU::Z80::BusOperation::Input: {
			uint8_t value = 0xff;
			if((address&7) == 6) {
				set_vsync(true);
				line_counter_ = 0;

				uint16_t mask = 0x100;
				for(int c = 0; c < 8; c++) {
					if(!(address & mask)) value &= key_states_[c];
					mask <<= 1;
				}
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
//	cycles_since_display_update_ = 0;
}

void Machine::set_vsync(bool sync) {
	if(sync == vsync_) return;
	vsync_ = sync;
	update_sync();
}

void Machine::set_hsync(bool sync) {
	if(sync == hsync_) return;
	hsync_ = sync;
	update_sync();
}

void Machine::update_sync() {
	bool is_sync = hsync_ || vsync_;
	if(is_sync == is_sync_) return;

	if(line_data_) {
		output_data();
	}

	if(is_sync_) {
		crt_->output_sync(cycles_since_display_update_ << 1);
	} else {
		output_level(cycles_since_display_update_ << 1);
	}
	cycles_since_display_update_ = 0;
	is_sync_ = is_sync;
}

void Machine::output_level(unsigned int number_of_cycles) {
	uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
	if(colour_pointer) *colour_pointer = 0xff;
	crt_->output_level(number_of_cycles);
}

void Machine::output_data() {
	unsigned int data_length = (unsigned int)(line_data_pointer_ - line_data_);
	crt_->output_data(data_length, 1);
	line_data_pointer_ = line_data_ = nullptr;
	cycles_since_display_update_ -= data_length >> 1;
}

void Machine::output_byte(uint8_t byte) {
	if(line_data_) {
		if(cycles_since_display_update_ > 4) {
			output_data();
		}
	} else {
		output_level(cycles_since_display_update_ << 1);
		cycles_since_display_update_ = 0;
	}

	if(!line_data_) {
		line_data_pointer_ = line_data_ = crt_->allocate_write_area(320);
	}

	if(line_data_) {
		uint8_t mask = 0x80;
		for(int c = 0; c < 8; c++) {
			line_data_pointer_[c] = (byte & mask) ? 0xff : 0x00;
			mask >>= 1;
		}
		line_data_pointer_ += 8;

		if(line_data_pointer_ - line_data_ == 320) {
			output_data();
		}
	}
}

void Machine::setup_output(float aspect_ratio) {
	crt_.reset(new Outputs::CRT::CRT(210 * 2, 1, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"return float(texture(texID, coordinate).r) / 255.0;"
		"}");
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
