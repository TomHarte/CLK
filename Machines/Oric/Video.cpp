//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Oric;

VideoOutput::VideoOutput(uint8_t *memory) :
	_ram(memory),
	_frame_counter(0), _counter(0),
	_state(Sync), _cycles_in_state(0)
{
	_crt.reset(new Outputs::CRT::CRT(64*6, 6, Outputs::CRT::DisplayType::PAL50, 1));

	// TODO: this is a copy and paste from the Electron; factor out.
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue >>= 4 - (int(icoordinate.x * 8) & 4);"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");
}

std::shared_ptr<Outputs::CRT::CRT> VideoOutput::get_crt()
{
	return _crt;
}

void VideoOutput::run_for_cycles(int number_of_cycles)
{
	// Vertical: 0–39: pixels; otherwise blank; 48–53 sync
	// Horizontal: 0–223: pixels; otherwise blank; 256–259 sync

	while(number_of_cycles--)
	{
		_counter = (_counter + 1)%(312 * 64);	// TODO: NTSC

		State new_state = Blank;
		int h_counter =_counter & 63;
		if(
			(h_counter >= 48 && h_counter <= 53) ||
			(_counter >= 256*64 && _counter <= 259*64)) new_state = Sync;
		else if(_counter < 224*64 && h_counter < 40) new_state = Pixels;

		if(_state != new_state)
		{
			switch(_state)
			{
				case Sync:		_crt->output_sync(_cycles_in_state * 6);		break;
				case Blank:		_crt->output_blank(_cycles_in_state * 6);		break;
				case Pixels:	_crt->output_data(_cycles_in_state * 6, 2);		break;
			}
			_state = new_state;
			_cycles_in_state = 0;
			if(_state == Pixels) _pixel_target = _crt->allocate_write_area(120);
		}
		_cycles_in_state++;

		if(new_state == Pixels) {
			_pixel_target[0] = 0x70;
			_pixel_target[1] = 0x14;
			_pixel_target[2] = 0x23;
			_pixel_target += 3;
		}
	}
}
