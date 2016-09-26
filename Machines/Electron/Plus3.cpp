//
//  Plus3.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Plus3.hpp"

using namespace Electron;

void Plus3::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive)
{
	if(!_drives[drive]) _drives[drive].reset(new Storage::Disk::Drive);
	_drives[drive]->set_disk(disk);
}

void Plus3::set_control_register(uint8_t control)
{
	// TODO:
	//	bit 0 => enable or disable drive 1
	//	bit 1 => enable or disable drive 2
	//	bit 2 => side select
	//	bit 3 => single density select
	switch(control&3)
	{
		case 0:		set_drive(nullptr);		break;
		default:	set_drive(_drives[0]);	break;
		case 2:		set_drive(_drives[1]);	break;
	}
	if(_drives[0]) _drives[0]->set_head((control & 0x04) ? 1 : 0);
	if(_drives[1]) _drives[1]->set_head((control & 0x04) ? 1 : 0);
	set_is_double_density(!(control & 0x08));
}
