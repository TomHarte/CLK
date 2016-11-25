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
	cycles_since_video_update_(0),
	use_fast_tape_hack_(false),
	typer_delay_(2500000),
	keyboard_read_count_(0),
	keyboard_(new Keyboard),
	ram_top_(0xbfff),
	paged_rom_(rom_)
{
	set_clock_rate(1000000);
	via_.set_interrupt_delegate(this);
	via_.keyboard = keyboard_;
	clear_all_keys();
	via_.tape->set_delegate(this);
	Memory::Fuzz(ram_, sizeof(ram_));
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(target.tapes.size())
	{
		via_.tape->set_tape(target.tapes.front());
	}

	if(target.loadingCommand.length())	// TODO: and automatic loading option enabled
	{
		set_typer_for_string(target.loadingCommand.c_str());
	}

	if(target.oric.has_microdisc)
	{
		microdisc_is_enabled_ = true;
		microdisc_did_change_paging_flags(&microdisc_);
		microdisc_.set_delegate(this);
	}

	if(target.oric.use_atmos_rom)
	{
		memcpy(rom_, basic11_rom_.data(), std::min(basic11_rom_.size(), sizeof(rom_)));

		is_using_basic11_ = true;
		tape_get_byte_address_ = 0xe6c9;
		scan_keyboard_address_ = 0xf495;
		tape_speed_address_ = 0x024d;
	}
	else
	{
		memcpy(rom_, basic10_rom_.data(), std::min(basic10_rom_.size(), sizeof(rom_)));

		is_using_basic11_ = false;
		tape_get_byte_address_ = 0xe630;
		scan_keyboard_address_ = 0xf43c;
		tape_speed_address_ = 0x67;
	}
}

void Machine::set_rom(ROM rom, const std::vector<uint8_t> &data)
{
	switch(rom)
	{
		case BASIC11:	basic11_rom_ = std::move(data);		break;
		case BASIC10:	basic10_rom_ = std::move(data);		break;
		case Microdisc:	microdisc_rom_ = std::move(data);	break;
	}
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(address > ram_top_)
	{
		if(isReadOperation(operation)) *value = paged_rom_[address - ram_top_ - 1];

		// 024D = 0 => fast; otherwise slow
		// E6C9 = read byte: return byte in A
		if(address == tape_get_byte_address_ && paged_rom_ == rom_ && use_fast_tape_hack_ && operation == CPU6502::BusOperation::ReadOpcode && via_.tape->has_tape() && !via_.tape->get_tape()->is_at_end())
		{
			uint8_t next_byte = via_.tape->get_next_byte(!ram_[tape_speed_address_]);
			set_value_of_register(CPU6502::A, next_byte);
			set_value_of_register(CPU6502::Flags, next_byte ? 0 : CPU6502::Flag::Zero);
			*value = 0x60; // i.e. RTS
		}
	}
	else
	{
		if((address & 0xff00) == 0x0300)
		{
			if(microdisc_is_enabled_ && address >= 0x0310)
			{
				switch(address)
				{
					case 0x0310: case 0x0311: case 0x0312: case 0x0313:
						if(isReadOperation(operation)) *value = microdisc_.get_register(address);
						else microdisc_.set_register(address, *value);
					break;
					case 0x314:
						if(isReadOperation(operation)) *value = microdisc_.get_interrupt_request_register();
						else microdisc_.set_control_register(*value);
					break;
					case 0x318:
						if(isReadOperation(operation)) *value = microdisc_.get_data_request_register();
					break;
				}
			}
			else
			{
				if(isReadOperation(operation)) *value = via_.get_register(address);
				else via_.set_register(address, *value);
			}
		}
		else
		{
			if(isReadOperation(operation))
				*value = ram_[address];
			else
			{
				if(address >= 0x9800 && address <= 0xc000) { update_video(); typer_delay_ = 0; }
				ram_[address] = *value;
			}
		}
	}

	if(_typer && address == scan_keyboard_address_ && operation == CPU6502::BusOperation::ReadOpcode)
	{
		// the Oric 1 misses any key pressed on the very first entry into the read keyboard routine, so don't
		// do anything until at least the second, regardless of machine
		if(!keyboard_read_count_) keyboard_read_count_++;
		else if(!_typer->type_next_character())
		{
			clear_all_keys();
			_typer.reset();
		}
	}

	via_.run_for_cycles(1);
	cycles_since_video_update_++;
	return 1;
}

void Machine::synchronise()
{
	update_video();
	via_.synchronise();
}

void Machine::update_video()
{
	video_output_->run_for_cycles(cycles_since_video_update_);
	cycles_since_video_update_ = 0;
}

void Machine::setup_output(float aspect_ratio)
{
	video_output_.reset(new VideoOutput(ram_));
	via_.ay8910.reset(new GI::AY38910());
	via_.ay8910->set_clock_rate(1000000);
}

void Machine::close_output()
{
	video_output_.reset();
	via_.ay8910.reset();
}

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	set_interrupt_line();
}

void Machine::set_key_state(uint16_t key, bool isPressed)
{
	if(key == KeyNMI)
	{
		set_nmi_line(isPressed);
	}
	else
	{
		if(isPressed)
			keyboard_->rows[key >> 8] |= (key & 0xff);
		else
			keyboard_->rows[key >> 8] &= ~(key & 0xff);
	}
}

void Machine::clear_all_keys()
{
	memset(keyboard_->rows, 0, sizeof(keyboard_->rows));
}

void Machine::set_use_fast_tape_hack(bool activate)
{
	use_fast_tape_hack_ = activate;
}

void Machine::tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player)
{
	// set CB1
	via_.set_control_line_input(VIA::Port::B, VIA::Line::One, tape_player->get_input());
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt()
{
	return video_output_->get_crt();
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker()
{
	return via_.ay8910;
}

void Machine::run_for_cycles(int number_of_cycles)
{
	CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles);
}

#pragma mark - The 6522

Machine::VIA::VIA() :
	MOS::MOS6522<Machine::VIA>(),
	cycles_since_ay_update_(0),
	tape(new TapePlayer) {}

void Machine::VIA::set_control_line_output(Port port, Line line, bool value)
{
	if(line)
	{
		if(port) ay_bdir_ = value; else ay_bc1_ = value;
		update_ay();
	}
}

void Machine::VIA::set_port_output(Port port, uint8_t value, uint8_t direction_mask)
{
	if(port)
	{
		keyboard->row = value;
		tape->set_motor_control(value & 0x40);
	}
	else
	{
		ay8910->set_data_input(value);
	}
}

uint8_t Machine::VIA::get_port_input(Port port)
{
	if(port)
	{
		uint8_t column = ay8910->get_port_output(false) ^ 0xff;
		return (keyboard->rows[keyboard->row & 7] & column) ? 0x08 : 0x00;
	}
	else
	{
		return ay8910->get_data_output();
	}
}

void Machine::VIA::synchronise()
{
	ay8910->run_for_cycles(cycles_since_ay_update_);
	ay8910->flush();
	cycles_since_ay_update_ = 0;
}

void Machine::VIA::run_for_cycles(unsigned int number_of_cycles)
{
	cycles_since_ay_update_ += number_of_cycles;
	MOS::MOS6522<VIA>::run_for_cycles(number_of_cycles);
	tape->run_for_cycles((int)number_of_cycles);
}

void Machine::VIA::update_ay()
{
	ay8910->run_for_cycles(cycles_since_ay_update_);
	cycles_since_ay_update_ = 0;
	ay8910->set_control_lines( (GI::AY38910::ControlLines)((ay_bdir_ ? GI::AY38910::BCDIR : 0) | (ay_bc1_ ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
}

#pragma mark - TapePlayer

Machine::TapePlayer::TapePlayer() :
	Storage::Tape::BinaryTapePlayer(1000000)
{}

uint8_t Machine::TapePlayer::get_next_byte(bool fast)
{
	return (uint8_t)parser_.get_next_byte(get_tape(), fast);
}

#pragma mark - Microdisc

void Machine::microdisc_did_change_paging_flags(class Microdisc *microdisc)
{
	int flags = microdisc->get_paging_flags();
	if(!(flags&Microdisc::PagingFlags::BASICDisable))
	{
		ram_top_ = 0xbfff;
		paged_rom_ = rom_;
	}
	else
	{
		if(flags&Microdisc::PagingFlags::MicrodscDisable)
		{
			ram_top_ = 0xffff;
		}
		else
		{
			ram_top_ = 0xdfff;
			paged_rom_ = microdisc_rom_.data();
		}
	}
}

void Machine::wd1770_did_change_interrupt_request_status(WD::WD1770 *wd1770)
{
	set_interrupt_line();
}

void Machine::wd1770_did_change_data_request_status(WD::WD1770 *wd1770)
{
	// Don't care.
}

void Machine::set_interrupt_line()
{
	set_irq_line(
		via_.get_interrupt_line() ||
		(microdisc_is_enabled_ && microdisc_.get_interrupt_request_line()));
}
