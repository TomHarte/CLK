//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

using namespace Oric;

Machine::Machine()
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
			_ram[address] = *value;
	}

	return 1;
}

void Machine::setup_output(float aspect_ratio)
{
	// TODO: this is a copy and paste from the Electron; correct.

	_crt.reset(new Outputs::CRT::CRT(256, 8, Outputs::CRT::DisplayType::PAL50, 1));
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue >>= 4 - (int(icoordinate.x * 8) & 4);"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");
}

void Machine::close_output()
{
}
