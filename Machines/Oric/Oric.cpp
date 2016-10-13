//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

using namespace Oric;

Machine::Machine() : _cycles_since_video_update(0)
{
	set_clock_rate(1000000);
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
}

void Machine::set_rom(std::vector<uint8_t> data)
{
	memcpy(_rom, data.data(), std::min(data.size(), sizeof(_rom)));
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(address > 0xc000)
	{
		if(isReadOperation(operation)) *value = _rom[address&16383];
	}
	else
	{
		if(isReadOperation(operation))
			*value = _ram[address];
		else
		{
			if(address >= 0xa000) update_video();
			_ram[address] = *value;
		}
	}

	_cycles_since_video_update++;
	return 1;
}

void Machine::synchronise()
{
	update_video();
}

void Machine::update_video()
{
	_videoOutput->run_for_cycles(_cycles_since_video_update);
	_cycles_since_video_update = 0;
}

void Machine::setup_output(float aspect_ratio)
{
	_videoOutput.reset(new VideoOutput(_ram));
}

void Machine::close_output()
{
	_videoOutput.reset();
}
