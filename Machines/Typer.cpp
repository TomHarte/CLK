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
	_counter(-delay), _frequency(frequency), _string_pointer(0), _delegate(delegate), _phase(0)
{
	size_t string_size = strlen(string) + 3;
	 _string = (char *)malloc(string_size);
	snprintf(_string, strlen(string) + 3, "%c%s%c", Typer::BeginString, string, Typer::EndString);
}

void Typer::update(int duration)
{
	if(_string)
	{
		if(_counter < 0 && _counter + duration >= 0)
		{
			if(!type_next_character())
			{
				_delegate->typer_reset(this);
			}
		}

		_counter += duration;
		while(_string && _counter > _frequency)
		{
			_counter -= _frequency;
			if(!type_next_character())
			{
				_delegate->typer_reset(this);
			}
		}
	}
}

bool Typer::type_next_character()
{
	if(_string == nullptr) return false;

	if(_delegate->typer_set_next_character(this, _string[_string_pointer], _phase))
	{
		_phase = 0;
		if(!_string[_string_pointer])
		{
			free(_string);
			_string = nullptr;
			return false;
		}

		_string_pointer++;
	}
	else
	{
		_phase++;
	}

	return true;
}

Typer::~Typer()
{
	free(_string);
}

#pragma mark - Delegate

bool Typer::Delegate::typer_set_next_character(Utility::Typer *typer, char character, int phase)
{
	uint16_t *sequence = sequence_for_character(typer, character);
	if(!sequence) return true;

	if(!phase) clear_all_keys();
	else
	{
		set_key_state(sequence[phase - 1], true);
		return sequence[phase] == Typer::Delegate::EndSequence;
	}

	return false;
}

void Typer::Delegate::typer_reset(Typer *typer)
{
	clear_all_keys();
}

uint16_t *Typer::Delegate::sequence_for_character(Typer *typer, char character)
{
	return nullptr;
}
