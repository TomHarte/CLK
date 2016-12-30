//
//  Microdisc.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Microdisc.hpp"

using namespace Oric;

namespace {
	// The number below, in cycles against an 8Mhz clock, was arrived at fairly unscientifically,
	// by comparing the amount of time this emulator took to show a directory versus a video of
	// a real Oric. It therefore assumes all other timing measurements were correct on the day
	// of the test. More work to do, I think.
	const int head_load_request_counter_target = 7653333;
}

Microdisc::Microdisc() :
	irq_enable_(false),
	delegate_(nullptr),
	paging_flags_(BASICDisable),
	head_load_request_counter_(-1),
	WD1770(P1793),
	last_control_(0)
{
	set_control_register(last_control_, 0xff);
}

void Microdisc::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive)
{
	if(!drives_[drive])
	{
		drives_[drive].reset(new Storage::Disk::Drive);
		if(drive == selected_drive_) set_drive(drives_[drive]);
	}
	drives_[drive]->set_disk(disk);
}

void Microdisc::set_control_register(uint8_t control)
{
	uint8_t changes = last_control_ ^ control;
	last_control_ = control;
	set_control_register(control, changes);
}

void Microdisc::set_control_register(uint8_t control, uint8_t changes)
{
	// b2: data separator clock rate select (1 = double)	[TODO]

	// b65: drive select
	if((changes >> 5)&3)
	{
		selected_drive_ = (control >> 5)&3;
		set_drive(drives_[selected_drive_]);
	}

	// b4: side select
	if(changes & 0x10)
	{
		unsigned int head = (control & 0x10) ? 1 : 0;
		for(int c = 0; c < 4; c++)
		{
			if(drives_[c]) drives_[c]->set_head(head);
		}
	}

	// b3: double density select (0 = double)
	if(changes & 0x08)
	{
		set_is_double_density(!(control & 0x08));
	}

	// b0: IRQ enable
	if(changes & 0x01)
	{
		bool had_irq = get_interrupt_request_line();
		irq_enable_ = !!(control & 0x01);
		bool has_irq = get_interrupt_request_line();
		if(has_irq != had_irq && delegate_)
		{
			delegate_->wd1770_did_change_output(this);
		}
	}

	// b7: EPROM select (0 = select)
	// b1: ROM disable (0 = disable)
	if(changes & 0x82)
	{
		paging_flags_ = ((control & 0x02) ? 0 : BASICDisable) | ((control & 0x80) ? MicrodscDisable : 0);
		if(delegate_) delegate_->microdisc_did_change_paging_flags(this);
	}
}

bool Microdisc::get_interrupt_request_line()
{
	return irq_enable_ && WD1770::get_interrupt_request_line();
}

uint8_t Microdisc::get_interrupt_request_register()
{
	return 0x7f | (WD1770::get_interrupt_request_line() ? 0x00 : 0x80);
}

uint8_t Microdisc::get_data_request_register()
{
	return 0x7f | (get_data_request_line() ? 0x00 : 0x80);
}

void Microdisc::set_head_load_request(bool head_load)
{
	set_motor_on(head_load);
	if(head_load)
	{
		head_load_request_counter_ = 0;
	}
	else
	{
		head_load_request_counter_ = head_load_request_counter_target;
		set_head_loaded(head_load);
	}
}

void Microdisc::run_for_cycles(unsigned int number_of_cycles)
{
	if(head_load_request_counter_ < head_load_request_counter_target)
	{
		head_load_request_counter_ += number_of_cycles;
		if(head_load_request_counter_ >= head_load_request_counter_target) set_head_loaded(true);
	}
	WD::WD1770::run_for_cycles(number_of_cycles);
}

bool Microdisc::get_drive_is_ready()
{
	return true;
}
