//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Typer.hpp"
#include <stdlib.h>

using namespace Utility;

Typer::Typer(const char *string, int delay, int frequency, Delegate *delegate) :
	_counter(-delay), _frequency(frequency), _string(strdup(string)), _string_pointer(0), _delegate(delegate) {}

void Typer::update(int duration)
{
	if(_string)
	{
		if(_counter < 0 && _counter + duration >= 0)
		{
			_delegate->typer_set_next_character(this, _string[_string_pointer]);
			_string_pointer++;
		}

		_counter += duration;
		while(_counter > _frequency)
		{
			_counter -= _frequency;
			_delegate->typer_set_next_character(this, _string[_string_pointer]);
			_string_pointer++;

			if(!_string[_string_pointer])
			{
				free(_string);
				_string = nullptr;
				return;
			}
		}
	}
}

Typer::~Typer()
{
	free(_string);
}
