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
	// The clock rate is 3.25Mhz.
	const unsigned int ZX8081ClockRate = 3250000;
}

Machine::Machine() :
	vsync_(false),
	hsync_(false),
	nmi_is_enabled_(false),
	tape_player_(ZX8081ClockRate) {
	set_clock_rate(ZX8081ClockRate);
	tape_player_.set_motor_control(true);
	clear_all_keys();
}

int Machine::perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
	int wait_cycles = 0;

	int previous_counter = horizontal_counter_;
	horizontal_counter_ += cycle.length;

	if(previous_counter < vsync_start_cycle_ && horizontal_counter_ >= vsync_start_cycle_) {
		video_->run_for_cycles(vsync_start_cycle_ - previous_counter);
		set_hsync(true);
		if(nmi_is_enabled_) {
			set_non_maskable_interrupt_line(true);
			if(!get_halt_line()) {
				wait_cycles = vsync_end_cycle_ - horizontal_counter_;
			}
		}
		video_->run_for_cycles(horizontal_counter_ - vsync_start_cycle_ + wait_cycles);
	} else if(previous_counter <= vsync_end_cycle_ && horizontal_counter_ > vsync_end_cycle_) {
		video_->run_for_cycles(vsync_end_cycle_ - previous_counter);
		set_hsync(false);
		if(nmi_is_enabled_) set_non_maskable_interrupt_line(false);
		video_->run_for_cycles(horizontal_counter_ - vsync_end_cycle_);
	} else {
		video_->run_for_cycles(cycle.length);
	}

	horizontal_counter_ += wait_cycles;
	if(is_zx81_) horizontal_counter_ %= 207;

//	tape_player_.run_for_cycles(cycle.length + wait_cycles);

	uint16_t refresh = 0;
	uint16_t address = cycle.address ? *cycle.address : 0;
	bool is_opcode_read = false;
	switch(cycle.operation) {
		case CPU::Z80::PartialMachineCycle::Output:
			set_vsync(false);
			line_counter_ = 0;

			if(!(address & 2)) nmi_is_enabled_ = false;
			if(!(address & 1)) nmi_is_enabled_ = is_zx81_;
		break;

		case CPU::Z80::PartialMachineCycle::Input: {
			uint8_t value = 0xff;
			if(!(address&1)) {
				set_vsync(true);

				uint16_t mask = 0x100;
				for(int c = 0; c < 8; c++) {
					if(!(address & mask)) value &= key_states_[c];
					mask <<= 1;
				}

				value &= ~(tape_player_.get_input() ? 0x00 : 0x80);
			}
			*cycle.value = value;
		} break;

		case CPU::Z80::PartialMachineCycle::Interrupt:
			line_counter_ = (line_counter_ + 1) & 7;
			*cycle.value = 0xff;
			horizontal_counter_ = 0;
		break;

		case CPU::Z80::PartialMachineCycle::ReadOpcodeStart:
		case CPU::Z80::PartialMachineCycle::ReadOpcodeWait:
			// The ZX80 and 81 signal an interrupt while refresh is active and bit 6 of the refresh
			// address is low. The Z80 signals a refresh, providing the refresh address during the
			// final two cycles of an opcode fetch. Therefore communicate a transient signalling
			// of the IRQ line if necessary.
			refresh = get_value_of_register(CPU::Z80::Register::Refresh);
			set_interrupt_line(!(refresh & 0x40), -2);
			set_interrupt_line(false);

			// Check for use of the fast tape hack.
			if(address == tape_trap_address_) { // TODO: && fast_tape_hack_enabled_
				int next_byte = parser_.get_next_byte(tape_player_.get_tape());
				if(next_byte != -1) {
					uint16_t hl = get_value_of_register(CPU::Z80::Register::HL);
					ram_[hl & ram_mask_] = (uint8_t)next_byte;
					*cycle.value = 0x00;
					set_value_of_register(CPU::Z80::Register::ProgramCounter, tape_return_address_ - 1);
					return 0;
				}
			}
			is_opcode_read = true;

		case CPU::Z80::PartialMachineCycle::Read:
			if(address < ram_base_) {
				*cycle.value = rom_[address & rom_mask_];
			} else {
				uint8_t value = ram_[address & ram_mask_];

				// If this is an M1 cycle reading from above the 32kb mark and HALT is not
				// currently active, perform a video output and return a NOP. Otherwise,
				// just return the value as read.
				if(is_opcode_read && address&0x8000 && !(value & 0x40) && !get_halt_line()) {
					size_t char_address = (size_t)((refresh & 0xff00) | ((value & 0x3f) << 3) | line_counter_);
					if(char_address < ram_base_) {
						uint8_t mask = (value & 0x80) ? 0x00 : 0xff;
						value = rom_[char_address & rom_mask_] ^ mask;
					}

					video_->output_byte(value);
					*cycle.value = 0;
				} else *cycle.value = value;
			}
		break;

		case CPU::Z80::PartialMachineCycle::Write:
			if(address >= ram_base_) {
				ram_[address & ram_mask_] = *cycle.value;
			}
		break;

		default: break;
	}

	return wait_cycles;
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
	is_zx81_ = target.zx8081.isZX81;
	if(is_zx81_) {
		rom_ = zx81_rom_;
		tape_trap_address_ = 0x37c;
		tape_return_address_ = 0x380;
		vsync_start_cycle_ = 13;
		vsync_end_cycle_ = 33;
		vsync_start_cycle_ = 16;
		vsync_end_cycle_ = 32;
	} else {
		rom_ = zx80_rom_;
		tape_trap_address_ = 0x220;
		tape_return_address_ = 0x248;
		vsync_start_cycle_ = 13;
		vsync_end_cycle_ = 33;
	}
	rom_mask_ = (uint16_t)(rom_.size() - 1);

	switch(target.zx8081.memory_model) {
		case StaticAnalyser::ZX8081MemoryModel::Unexpanded:
			ram_.resize(1024);
			ram_base_ = 16384;
			ram_mask_ = 1023;
		break;
		case StaticAnalyser::ZX8081MemoryModel::SixteenKB:
			ram_.resize(16384);
			ram_base_ = 16384;
			ram_mask_ = 16383;
		break;
		case StaticAnalyser::ZX8081MemoryModel::SixtyFourKB:
			ram_.resize(65536);
			ram_base_ = 8192;
			ram_mask_ = 65535;
		break;
	}
	Memory::Fuzz(ram_);

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
