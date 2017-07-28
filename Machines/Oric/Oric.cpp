//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"
#include "../MemoryFuzzer.hpp"

using namespace Oric;

Machine::Machine() :
		use_fast_tape_hack_(false),
		typer_delay_(2500000),
		keyboard_read_count_(0),
		keyboard_(new Keyboard),
		ram_top_(0xbfff),
		paged_rom_(rom_),
		microdisc_is_enabled_(false) {
	set_clock_rate(1000000);
	via_.set_interrupt_delegate(this);
	via_.keyboard = keyboard_;
	clear_all_keys();
	via_.tape->set_delegate(this);
	Memory::Fuzz(ram_, sizeof(ram_));
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	if(target.tapes.size()) {
		via_.tape->set_tape(target.tapes.front());
	}

	if(target.loadingCommand.length()) {	// TODO: and automatic loading option enabled
		set_typer_for_string(target.loadingCommand.c_str());
	}

	if(target.oric.has_microdisc) {
		microdisc_is_enabled_ = true;
		microdisc_did_change_paging_flags(&microdisc_);
		microdisc_.set_delegate(this);
	}

	int drive_index = 0;
	for(auto disk : target.disks) {
		if(drive_index < 4) microdisc_.set_disk(disk, drive_index);
		drive_index++;
	}

	if(target.oric.use_atmos_rom) {
		memcpy(rom_, basic11_rom_.data(), std::min(basic11_rom_.size(), sizeof(rom_)));

		is_using_basic11_ = true;
		tape_get_byte_address_ = 0xe6c9;
		scan_keyboard_address_ = 0xf495;
		tape_speed_address_ = 0x024d;
	} else {
		memcpy(rom_, basic10_rom_.data(), std::min(basic10_rom_.size(), sizeof(rom_)));

		is_using_basic11_ = false;
		tape_get_byte_address_ = 0xe630;
		scan_keyboard_address_ = 0xf43c;
		tape_speed_address_ = 0x67;
	}
}

void Machine::set_rom(ROM rom, const std::vector<uint8_t> &data) {
	switch(rom) {
		case BASIC11:	basic11_rom_ = std::move(data);		break;
		case BASIC10:	basic10_rom_ = std::move(data);		break;
		case Microdisc:	microdisc_rom_ = std::move(data);	break;
		case Colour:
			colour_rom_ = std::move(data);
			if(video_output_) video_output_->set_colour_rom(colour_rom_);
		break;
	}
}

Cycles Machine::perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	if(address > ram_top_) {
		if(isReadOperation(operation)) *value = paged_rom_[address - ram_top_ - 1];

		// 024D = 0 => fast; otherwise slow
		// E6C9 = read byte: return byte in A
		if(address == tape_get_byte_address_ && paged_rom_ == rom_ && use_fast_tape_hack_ && operation == CPU::MOS6502::BusOperation::ReadOpcode && via_.tape->has_tape() && !via_.tape->get_tape()->is_at_end()) {
			uint8_t next_byte = via_.tape->get_next_byte(!ram_[tape_speed_address_]);
			set_value_of_register(CPU::MOS6502::A, next_byte);
			set_value_of_register(CPU::MOS6502::Flags, next_byte ? 0 : CPU::MOS6502::Flag::Zero);
			*value = 0x60; // i.e. RTS
		}
	} else {
		if((address & 0xff00) == 0x0300) {
			if(microdisc_is_enabled_ && address >= 0x0310) {
				switch(address) {
					case 0x0310: case 0x0311: case 0x0312: case 0x0313:
						if(isReadOperation(operation)) *value = microdisc_.get_register(address);
						else microdisc_.set_register(address, *value);
					break;
					case 0x314: case 0x315: case 0x316: case 0x317:
						if(isReadOperation(operation)) *value = microdisc_.get_interrupt_request_register();
						else microdisc_.set_control_register(*value);
					break;
					case 0x318: case 0x319: case 0x31a: case 0x31b:
						if(isReadOperation(operation)) *value = microdisc_.get_data_request_register();
					break;
				}
			} else {
				if(isReadOperation(operation)) *value = via_.get_register(address);
				else via_.set_register(address, *value);
			}
		} else {
			if(isReadOperation(operation))
				*value = ram_[address];
			else {
				if(address >= 0x9800 && address <= 0xc000) { update_video(); typer_delay_ = 0; }
				ram_[address] = *value;
			}
		}
	}

	if(typer_ && address == scan_keyboard_address_ && operation == CPU::MOS6502::BusOperation::ReadOpcode) {
		// the Oric 1 misses any key pressed on the very first entry into the read keyboard routine, so don't
		// do anything until at least the second, regardless of machine
		if(!keyboard_read_count_) keyboard_read_count_++;
		else if(!typer_->type_next_character()) {
			clear_all_keys();
			typer_.reset();
		}
	}

	via_.run_for(Cycles(1));
	if(microdisc_is_enabled_) microdisc_.run_for(Cycles(8));
	cycles_since_video_update_++;
	return Cycles(1);
}

void Machine::flush() {
	update_video();
	via_.flush();
}

void Machine::update_video() {
	video_output_->run_for(cycles_since_video_update_.flush());
}

void Machine::setup_output(float aspect_ratio) {
	via_.ay8910.reset(new GI::AY38910());
	via_.ay8910->set_clock_rate(1000000);
	video_output_.reset(new VideoOutput(ram_));
	if(!colour_rom_.empty()) video_output_->set_colour_rom(colour_rom_);
}

void Machine::close_output() {
	video_output_.reset();
	via_.ay8910.reset();
}

void Machine::mos6522_did_change_interrupt_status(void *mos6522) {
	set_interrupt_line();
}

void Machine::set_key_state(uint16_t key, bool isPressed) {
	if(key == KeyNMI) {
		set_nmi_line(isPressed);
	} else {
		if(isPressed)
			keyboard_->rows[key >> 8] |= (key & 0xff);
		else
			keyboard_->rows[key >> 8] &= ~(key & 0xff);
	}
}

void Machine::clear_all_keys() {
	memset(keyboard_->rows, 0, sizeof(keyboard_->rows));
}

void Machine::set_use_fast_tape_hack(bool activate) {
	use_fast_tape_hack_ = activate;
}

void Machine::set_output_device(Outputs::CRT::OutputDevice output_device) {
	video_output_->set_output_device(output_device);
}

void Machine::tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player) {
	// set CB1
	via_.set_control_line_input(VIA::Port::B, VIA::Line::One, !tape_player->get_input());
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt() {
	return video_output_->get_crt();
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker() {
	return via_.ay8910;
}

void Machine::run_for(const Cycles cycles) {
	CPU::MOS6502::Processor<Machine>::run_for(cycles);
}

#pragma mark - The 6522

Machine::VIA::VIA() :
		MOS::MOS6522<Machine::VIA>(),
		tape(new TapePlayer) {}

void Machine::VIA::set_control_line_output(Port port, Line line, bool value) {
	if(line) {
		if(port) ay_bdir_ = value; else ay_bc1_ = value;
		update_ay();
	}
}

void Machine::VIA::set_port_output(Port port, uint8_t value, uint8_t direction_mask) {
	if(port) {
		keyboard->row = value;
		tape->set_motor_control(value & 0x40);
	} else {
		ay8910->set_data_input(value);
	}
}

uint8_t Machine::VIA::get_port_input(Port port) {
	if(port) {
		uint8_t column = ay8910->get_port_output(false) ^ 0xff;
		return (keyboard->rows[keyboard->row & 7] & column) ? 0x08 : 0x00;
	} else {
		return ay8910->get_data_output();
	}
}

void Machine::VIA::flush() {
	ay8910->run_for(cycles_since_ay_update_.flush());
	ay8910->flush();
}

void Machine::VIA::run_for(const Cycles cycles) {
	cycles_since_ay_update_ += cycles;
	MOS::MOS6522<VIA>::run_for(cycles);
	tape->run_for(cycles);
}

void Machine::VIA::update_ay() {
	ay8910->run_for(cycles_since_ay_update_.flush());
	ay8910->set_control_lines( (GI::AY38910::ControlLines)((ay_bdir_ ? GI::AY38910::BCDIR : 0) | (ay_bc1_ ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
}

#pragma mark - TapePlayer

Machine::TapePlayer::TapePlayer() :
		Storage::Tape::BinaryTapePlayer(1000000) {}

uint8_t Machine::TapePlayer::get_next_byte(bool fast) {
	return (uint8_t)parser_.get_next_byte(get_tape(), fast);
}

#pragma mark - Microdisc

void Machine::microdisc_did_change_paging_flags(class Microdisc *microdisc) {
	int flags = microdisc->get_paging_flags();
	if(!(flags&Microdisc::PagingFlags::BASICDisable)) {
		ram_top_ = 0xbfff;
		paged_rom_ = rom_;
	} else {
		if(flags&Microdisc::PagingFlags::MicrodscDisable) {
			ram_top_ = 0xffff;
		} else {
			ram_top_ = 0xdfff;
			paged_rom_ = microdisc_rom_.data();
		}
	}
}

void Machine::wd1770_did_change_output(WD::WD1770 *wd1770) {
	set_interrupt_line();
}

void Machine::set_interrupt_line() {
	set_irq_line(
		via_.get_interrupt_line() ||
		(microdisc_is_enabled_ && microdisc_.get_interrupt_request_line()));
}
