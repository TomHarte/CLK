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
	tape_player_(ZX8081ClockRate),
	use_fast_tape_hack_(false),
	tape_advance_delay_(0),
	has_latched_video_byte_(false) {
	set_clock_rate(ZX8081ClockRate);
	clear_all_keys();
}

int Machine::perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
	int previous_counter = horizontal_counter_;
	horizontal_counter_ += cycle.length;

	if(previous_counter < vsync_start_cycle_ && horizontal_counter_ >= vsync_start_cycle_) {
		video_->run_for(Cycles(vsync_start_cycle_ - previous_counter));
		set_hsync(true);
		line_counter_ = (line_counter_ + 1) & 7;
		if(nmi_is_enabled_) {
			set_non_maskable_interrupt_line(true);
		}
		video_->run_for(Cycles(horizontal_counter_ - vsync_start_cycle_));
	} else if(previous_counter < vsync_end_cycle_ && horizontal_counter_ >= vsync_end_cycle_) {
		video_->run_for(Cycles(vsync_end_cycle_ - previous_counter));
		set_hsync(false);
		if(nmi_is_enabled_) {
			set_non_maskable_interrupt_line(false);
			set_wait_line(false);
		}
		video_->run_for(Cycles(horizontal_counter_ - vsync_end_cycle_));
	} else {
		video_->run_for(Cycles(cycle.length));
	}

	if(is_zx81_) horizontal_counter_ %= 207;
	if(!tape_advance_delay_) {
		tape_player_.run_for_cycles(cycle.length);
	} else {
		tape_advance_delay_ = std::max(tape_advance_delay_ - cycle.length, 0);
	}

	if(nmi_is_enabled_ && !get_halt_line() && get_non_maskable_interrupt_line()) {
		set_wait_line(true);
	}

	if(!cycle.is_terminal()) {
		return 0;
	}

	uint16_t address = cycle.address ? *cycle.address : 0;
	bool is_opcode_read = false;
	switch(cycle.operation) {
		case CPU::Z80::PartialMachineCycle::Output:
			if(!(address & 2)) nmi_is_enabled_ = false;
			if(!(address & 1)) nmi_is_enabled_ = is_zx81_;
			if(!nmi_is_enabled_) {
				// Line counter reset is held low while vsync is active; simulate that lazily by performing
				// an instant reset upon the transition from active to inactive.
				if(vsync_) line_counter_ = 0;
				set_vsync(false);
			}
		break;

		case CPU::Z80::PartialMachineCycle::Input: {
			uint8_t value = 0xff;
			if(!(address&1)) {
				if(!nmi_is_enabled_) set_vsync(true);

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
			*cycle.value = 0xff;
			horizontal_counter_ = 0;
		break;

		case CPU::Z80::PartialMachineCycle::Refresh:
			// The ZX80 and 81 signal an interrupt while refresh is active and bit 6 of the refresh
			// address is low. The Z80 signals a refresh, providing the refresh address during the
			// final two cycles of an opcode fetch. Therefore communicate a transient signalling
			// of the IRQ line if necessary.
			if(!(address & 0x40)) {
				set_interrupt_line(true, -2);
				set_interrupt_line(false);
			}
			if(has_latched_video_byte_) {
				size_t char_address = (size_t)((address & 0xfe00) | ((latched_video_byte_ & 0x3f) << 3) | line_counter_);
				uint8_t mask = (latched_video_byte_ & 0x80) ? 0x00 : 0xff;
				if(char_address < ram_base_) {
					latched_video_byte_ = rom_[char_address & rom_mask_] ^ mask;
				} else {
					latched_video_byte_ = ram_[address & ram_mask_] ^ mask;
				}

				video_->output_byte(latched_video_byte_);
				has_latched_video_byte_ = false;
			}
		break;

		case CPU::Z80::PartialMachineCycle::ReadOpcodeStart:
		case CPU::Z80::PartialMachineCycle::ReadOpcodeWait:
			// Check for use of the fast tape hack.
			if(use_fast_tape_hack_ && address == tape_trap_address_ && tape_player_.has_tape()) {
				uint64_t prior_offset = tape_player_.get_tape()->get_offset();
				int next_byte = parser_.get_next_byte(tape_player_.get_tape());
				if(next_byte != -1) {
					uint16_t hl = get_value_of_register(CPU::Z80::Register::HL);
					ram_[hl & ram_mask_] = (uint8_t)next_byte;
					*cycle.value = 0x00;
					set_value_of_register(CPU::Z80::Register::ProgramCounter, tape_return_address_ - 1);

					// Assume that having read one byte quickly, we're probably going to be asked to read
					// another shortly. Therefore, temporarily disable the tape motor for 1000 cycles in order
					// to avoid fighting with real time. This is a stop-gap fix.
					tape_advance_delay_ = 1000;
					return 0;
				} else {
					tape_player_.get_tape()->set_offset(prior_offset);
				}
			}

			// Check for automatic tape control.
			if(use_automatic_tape_motor_control_) {
				tape_player_.set_motor_control((address >= automatic_tape_motor_start_address_) && (address < automatic_tape_motor_end_address_));
			}
			is_opcode_read = true;

		case CPU::Z80::PartialMachineCycle::Read:
			if(address < ram_base_) {
				*cycle.value = rom_[address & rom_mask_];
			} else {
				uint8_t value = ram_[address & ram_mask_];

				// If this is an M1 cycle reading from above the 32kb mark and HALT is not
				// currently active, latch for video output and return a NOP. Otherwise,
				// just return the value as read.
				if(is_opcode_read && address&0x8000 && !(value & 0x40) && !get_halt_line()) {
					latched_video_byte_ = value;
					has_latched_video_byte_ = true;
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

	if(typer_) typer_->update(cycle.length);

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

void Machine::run_for(const Cycles &cycles) {
	CPU::Z80::Processor<Machine>::run_for_cycles(int(cycles));
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	is_zx81_ = target.zx8081.isZX81;
	if(is_zx81_) {
		rom_ = zx81_rom_;
		tape_trap_address_ = 0x37c;
		tape_return_address_ = 0x380;
		vsync_start_cycle_ = 16;
		vsync_end_cycle_ = 32;
		automatic_tape_motor_start_address_ = 0x0340;
		automatic_tape_motor_end_address_ = 0x03c3;
	} else {
		rom_ = zx80_rom_;
		tape_trap_address_ = 0x220;
		tape_return_address_ = 0x248;
		vsync_start_cycle_ = 13;
		vsync_end_cycle_ = 33;
		automatic_tape_motor_start_address_ = 0x0206;
		automatic_tape_motor_end_address_ = 0x024d;
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

	if(target.loadingCommand.length()) {
		set_typer_for_string(target.loadingCommand.c_str());
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
