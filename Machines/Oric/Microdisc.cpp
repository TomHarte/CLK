//
//  Microdisc.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Microdisc.hpp"

using namespace Oric;

Microdisc::Microdisc() :
	irq_enable_(false),
	delegate_(nullptr),
	paging_flags_(BASICDisable)
{}

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
	// b2: data separator clock rate select (1 = double)	[TODO]

	// b65: drive select
	selected_drive_ = (control >> 5)&3;
	set_drive(drives_[selected_drive_]);

	// b4: side select
	for(int c = 0; c < 4; c++)
	{
		if(drives_[c]) drives_[c]->set_head((control & 0x10) ? 1 : 0);
	}

	// b3: double density select (0 = double)
	set_is_double_density(!(control & 0x08));

	// b0: IRQ enable
	irq_enable_ = !(control & 0x01);

	// b7: EPROM select (0 = select)
	// b1: ROM disable (0 = disable)
	int new_paging_flags = ((control & 0x02) ? 0 : BASICDisable) | ((control & 0x80) ? MicrodscDisable : 0);
	if(new_paging_flags != paging_flags_)
	{
		paging_flags_ = new_paging_flags;
		if(delegate_) delegate_->microdisc_did_change_paging_flags(this);
	}
}

bool Microdisc::get_interrupt_request_line()
{
	return irq_enable_ && WD1770::get_interrupt_request_line();
}

uint8_t Microdisc::get_interrupt_request_register()
{
	return get_interrupt_request_line() ? 0x00 : 0x80;
}

uint8_t Microdisc::get_data_request_register()
{
	return get_data_request_line() ? 0x00 : 0x80;
}
