//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"
#include "TapeUEF.hpp"

#include <algorithm>
#include <cassert>

using namespace Electron;

static const unsigned int cycles_per_line = 128;
static const unsigned int cycles_per_frame = 312*cycles_per_line + 64;
static const unsigned int crt_cycles_multiplier = 8;
static const unsigned int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

static const int first_graphics_line = 38;
static const int first_graphics_cycle = 33;
static const int last_graphics_cycle = 80 + first_graphics_cycle;

Machine::Machine() :
	_interrupt_control(0),
	_interrupt_status(Interrupt::PowerOnReset),
	_fieldCycles(0),
	_displayOutputPosition(0),
	_audioOutputPosition(0),
	_audioOutputPositionError(0),
	_currentOutputLine(0),
	_is_odd_field(false),
	_crt(std::unique_ptr<Outputs::CRT>(new Outputs::CRT(crt_cycles_per_line, 8, Outputs::CRT::DisplayType::PAL50, 1, 1)))
{
	_crt->set_rgb_sampling_function(
		"vec4 rgb_sample(vec2 coordinate)"
		"{"
			"float texValue = texture(texID, coordinate).r;"
			"return vec4(step(4.0/256.0, mod(texValue, 8.0/256.0)), step(2.0/256.0, mod(texValue, 4.0/256.0)), step(1.0/256.0, mod(texValue, 2.0/256.0)), 1.0);"
		"}");
	_crt->set_output_device(Outputs::CRT::Monitor);
//	_crt->set_visible_area(Outputs::Rect(0.23108f, 0.0f, 0.8125f, 0.98f));	//1875

	memset(_key_states, 0, sizeof(_key_states));
	memset(_palette, 0xf, sizeof(_palette));
	for(int c = 0; c < 16; c++)
		memset(_roms[c], 0xff, 16384);

	_speaker.set_input_rate(125000);
	_tape.set_delegate(this);
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	unsigned int cycles = 1;

	if(address < 0x8000)
	{
		if(isReadOperation(operation))
		{
			*value = _ram[address];
		}
		else
		{
			// If we're still before the display will start to be painted, or the address is
			// less than both the current line address and 0x3000, (the minimum screen mode
			// base address) then there's no way this write can affect the current frame. Sp
			// no need to flush the display. Otherwise, output up until now so that any
			// write doesn't have retroactive effect on the video output.
			if(!(
				(_fieldCycles < first_graphics_line * cycles_per_line) ||
				(address < _startLineAddress && address < 0x3000)
			))
				update_display();

			_ram[address] = *value;
		}

		// for the entire frame, RAM is accessible only on odd cycles; in modes below 4
		// it's also accessible only outside of the pixel regions
		cycles += (_fieldCycles&1)^1;
		if(_screen_mode < 4)
		{
			const int current_line = _fieldCycles >> 7;
			const int line_position = _fieldCycles & 127;
			if(current_line >= first_graphics_line && current_line < first_graphics_line+256 && line_position >= first_graphics_cycle && line_position < first_graphics_cycle + 80)
				cycles = (unsigned int)(80 + first_graphics_cycle - line_position);
		}
	}
	else
	{
		if(address >= 0xc000)
		{
			if((address & 0xff00) == 0xfe00)
			{
				cycles += (_fieldCycles&1)^1;
//				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

				switch(address&0xf)
				{
					case 0x0:
						if(isReadOperation(operation))
						{
							*value = _interrupt_status;
							_interrupt_status &= ~0x02;
						}
						else
						{
							_interrupt_control = *value;
							evaluate_interrupts();
						}
					break;
					case 0x1:
					break;
					case 0x2:
						printf("%02x to [2] mutates %04x ", *value, _startScreenAddress);
						_startScreenAddress = (_startScreenAddress & 0xfe00) | (uint16_t)(((*value) & 0xe0) << 1);
						printf("into %04x\n", _startScreenAddress);
					break;
					case 0x3:
						printf("%02x to [3] mutates %04x ", *value, _startScreenAddress);
						_startScreenAddress = (_startScreenAddress & 0x01ff) | (uint16_t)(((*value) & 0x3f) << 9);
						printf("into %04x\n", _startScreenAddress);
					break;
					case 0x4:
						if(isReadOperation(operation))
						{
							*value = _tape.get_data_register();
							_tape.clear_interrupts(Interrupt::ReceiveDataFull);
						}
						else
						{
							_tape.set_data_register(*value);
							_tape.clear_interrupts(Interrupt::TransmitDataEmpty);
						}
					break;
					case 0x5:
						if(!isReadOperation(operation))
						{
							const uint8_t interruptDisable = (*value)&0xf0;
							if( interruptDisable )
							{
								if( interruptDisable&0x10 ) _interrupt_status &= ~Interrupt::DisplayEnd;
								if( interruptDisable&0x20 ) _interrupt_status &= ~Interrupt::RealTimeClock;
								if( interruptDisable&0x40 ) _interrupt_status &= ~Interrupt::HighToneDetect;
								evaluate_interrupts();

								// TODO: NMI (?)
							}
//							else
							{
								uint8_t nextROM = (*value)&0xf;

//								if(nextROM&0x08)
//								{
//									_activeRom = (Electron::ROMSlot)(nextROM&0x0e);
//									printf("%d -> Paged %d\n", nextROM, _activeRom);
//								}
								if(((_active_rom&12) != 8) || (nextROM&8))
								{
									_active_rom = (Electron::ROMSlot)nextROM;
								}
//								else
//								{
//									printf("Ignored!");
//								}
//								printf("%d -> Paged %d\n", nextROM, _activeRom);
							}
						}
					break;
					case 0x6:
						if(!isReadOperation(operation))
						{
							update_audio();
							_speaker.set_divider(*value);
							_tape.set_counter(*value);
						}
					break;
					case 0x7:
						if(!isReadOperation(operation))
						{
							// update screen mode
							uint8_t new_screen_mode = ((*value) >> 3)&7;
							if(new_screen_mode == 7) new_screen_mode = 4;
							if(new_screen_mode != _screen_mode)
							{
								printf("To mode %d, %d cycles into field (%d)\n", new_screen_mode, _fieldCycles, _fieldCycles >> 7);
								update_display();
								_screen_mode = new_screen_mode;
								switch(_screen_mode)
								{
									case 0: case 1: case 2: _screenModeBaseAddress = 0x3000; break;
									case 3: _screenModeBaseAddress = 0x4000; break;
									case 4: case 5: _screenModeBaseAddress = 0x5800; break;
									case 6: _screenModeBaseAddress = 0x6000; break;
								}
							}

							// update speaker mode
							bool new_speaker_is_enabled = (*value & 6) == 2;
							if(new_speaker_is_enabled != _speaker.get_is_enabled())
							{
								update_audio();
								_speaker.set_is_enabled(new_speaker_is_enabled);
								_tape.set_is_enabled(!new_speaker_is_enabled);
							}

							_tape.set_is_running(((*value)&0x40) ? true : false);
							_tape.set_is_in_input_mode(((*value)&0x04) ? false : true);

							// TODO: caps lock LED
						}
					break;
					default:
					{
						if(!isReadOperation(operation))
						{
							update_display();

							static const int registers[4][4] = {
								{10, 8, 2, 0},
								{14, 12, 6, 4},
								{15, 13, 7, 5},
								{11, 9, 3, 1},
							};
							const int index = (address >> 1)&3;
							const uint8_t colour = ~(*value);
							if(address&1)
							{
								_palette[registers[index][0]]	= (_palette[registers[index][0]]&3)	| ((colour >> 1)&4);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&3)	| ((colour >> 0)&4);
								_palette[registers[index][2]]	= (_palette[registers[index][2]]&3)	| ((colour << 1)&4);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&3)	| ((colour << 2)&4);

								_palette[registers[index][2]]	= (_palette[registers[index][2]]&5)	| ((colour >> 4)&2);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&5)	| ((colour >> 3)&2);
							}
							else
							{
								_palette[registers[index][0]]	= (_palette[registers[index][0]]&6)	| ((colour >> 7)&1);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&6)	| ((colour >> 6)&1);
								_palette[registers[index][2]]	= (_palette[registers[index][2]]&6)	| ((colour >> 5)&1);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&6)	| ((colour >> 4)&1);

								_palette[registers[index][0]]	= (_palette[registers[index][0]]&5)	| ((colour >> 2)&2);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&5)	| ((colour >> 1)&2);
							}
						}
					}
					break;
				}
			}
			else
			{
				if(isReadOperation(operation))
					*value = _os[address & 16383];
			}
		}
		else
		{
			if(isReadOperation(operation))
			{
				switch(_active_rom)
				{
					case ROMSlotKeyboard:
					case ROMSlotKeyboard+1:
						*value = 0xf0;
						for(int address_line = 0; address_line < 14; address_line++)
						{
							if(!(address&(1 << address_line))) *value |= _key_states[address_line];
						}
					break;
					default:
						*value = _roms[_active_rom][address & 16383];
					break;
				}
			}
		}
	}

//	if(operation == CPU6502::BusOperation::ReadOpcode)
//	{
//		printf("%04x: %02x (%d)\n", address, *value, _fieldCycles);
//	}

	unsigned int start_of_graphics = get_first_graphics_cycle();
	const unsigned int real_time_clock_interrupt_time = start_of_graphics + 99*128;
	const unsigned int display_end_interrupt_time = start_of_graphics + 257*128 + 64;

	if(_fieldCycles < real_time_clock_interrupt_time && _fieldCycles + cycles >= real_time_clock_interrupt_time)
	{
		update_audio();
		signal_interrupt(Interrupt::RealTimeClock);
	}
	else if(_fieldCycles < display_end_interrupt_time && _fieldCycles + cycles >= display_end_interrupt_time)
	{
		update_audio();
		signal_interrupt(Interrupt::DisplayEnd);
	}

	_fieldCycles += cycles;

	switch(_fieldCycles)
	{
		case 64*128:
		case 196*128:
			update_audio();
		break;

		case cycles_per_frame:
			update_display();
			update_audio();
			_fieldCycles = 0;
			_displayOutputPosition = 0;
			_audioOutputPosition = 0;
			_currentOutputLine = 0;
		break;
	}

	_tape.run_for_cycles(cycles);

	return cycles;
}

void Machine::update_output()
{
	update_display();
	update_audio();
}

void Machine::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape.set_tape(tape);
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	switch(slot)
	{
		case ROMSlotOS:		target = _os;			break;
		default:			target = _roms[slot];	break;
	}

	memcpy(target, data, std::min((size_t)16384, length));
}

inline void Machine::signal_interrupt(Electron::Interrupt interrupt)
{
	_interrupt_status |= interrupt;
	evaluate_interrupts();
}

void Machine::tape_did_change_interrupt_status(Tape *tape)
{
	_interrupt_status = (_interrupt_status & ~(Interrupt::TransmitDataEmpty | Interrupt::ReceiveDataFull | Interrupt::HighToneDetect)) | _tape.get_interrupt_status();
	evaluate_interrupts();
}

inline void Machine::evaluate_interrupts()
{
	if(_interrupt_status & _interrupt_control)
	{
		_interrupt_status |= 1;
	}
	else
	{
		_interrupt_status &= ~1;
	}
	set_irq_line(_interrupt_status & 1);
}

inline void Machine::update_audio()
{
	int difference = (int)_fieldCycles - _audioOutputPosition;
	_audioOutputPosition = (int)_fieldCycles;
	_speaker.run_for_cycles((_audioOutputPositionError + difference) >> 4);
	_audioOutputPositionError = (_audioOutputPositionError + difference)&15;
}

inline void Machine::reset_pixel_output()
{
	display_x = 0;
	display_y = 0;
	_currentScreenAddress = _startLineAddress = _startScreenAddress;
	_currentOutputLine = 0;
}

inline void Machine::output_pixels(int start_x, int number_of_pixels)
{
	if(number_of_pixels)
	{
		if(
			((_screen_mode == 3) || (_screen_mode == 6)) &&
			(((display_y%10) >= 8) || (display_y >= 250))
		)
		{
			end_pixel_output();
			_crt->output_blank((unsigned int)number_of_pixels * crt_cycles_multiplier);
			return;
		}

		unsigned int newDivider = 0;
		switch(_screen_mode)
		{
			case 0: case 3:				newDivider = 1; break;
			case 1:	case 4: case 6:		newDivider = 2; break;
			case 2: case 5:				newDivider = 4; break;
		}
		bool is40Column = (_screen_mode > 3);

		if(!_writePointer || newDivider != _currentOutputDivider || _isOutputting40Columns != is40Column)
		{
			end_pixel_output();
			_currentOutputDivider = newDivider;
			_isOutputting40Columns = is40Column;
			_crt->allocate_write_area(640 / newDivider);
			_currentLine = _writePointer = _crt->get_write_target_for_buffer(0);
		}

		if(is40Column)
		{
			number_of_pixels = ((start_x + number_of_pixels) >> 1) - (start_x >> 1);
		}

#define GetNextPixels() \
	if(_currentScreenAddress&32768)\
	{\
		_currentScreenAddress = (_screenModeBaseAddress + _currentScreenAddress)&32767;\
	}\
	uint8_t pixels = _ram[_currentScreenAddress];\
	_currentScreenAddress = _currentScreenAddress+8
		switch(_screen_mode)
		{
			default:
			case 0: case 3: case 4: case 6:
				while(number_of_pixels--)
				{
					GetNextPixels();

					_writePointer[0] = _palette[(pixels&0x80) >> 4];
					_writePointer[1] = _palette[(pixels&0x40) >> 3];
					_writePointer[2] = _palette[(pixels&0x20) >> 2];
					_writePointer[3] = _palette[(pixels&0x10) >> 1];
					_writePointer[4] = _palette[(pixels&0x08) >> 0];
					_writePointer[5] = _palette[(pixels&0x04) << 1];
					_writePointer[6] = _palette[(pixels&0x02) << 2];
					_writePointer[7] = _palette[(pixels&0x01) << 3];

					_writePointer += 8;
				}
			break;

			case 1: case 5:
				while(number_of_pixels--)
				{
					GetNextPixels();

					_writePointer[0] = _palette[((pixels&0x80) >> 4) | ((pixels&0x08) >> 2)];
					_writePointer[1] = _palette[((pixels&0x40) >> 3) | ((pixels&0x04) >> 1)];
					_writePointer[2] = _palette[((pixels&0x20) >> 2) | ((pixels&0x02) >> 0)];
					_writePointer[3] = _palette[((pixels&0x10) >> 1) | ((pixels&0x01) << 1)];

					_writePointer += 4;
				}
			break;

			case 2:
				while(number_of_pixels--)
				{
					GetNextPixels();

					_writePointer[0] = _palette[((pixels&0x80) >> 4) | ((pixels&0x20) >> 3) | ((pixels&0x08) >> 2) | ((pixels&0x02) >> 1)];
					_writePointer[1] = _palette[((pixels&0x40) >> 3) | ((pixels&0x10) >> 2) | ((pixels&0x04) >> 1) | ((pixels&0x01) >> 0)];

					_writePointer += 2;
				}
			break;
		}
	}
#undef GetNextPixels
}

inline void Machine::end_pixel_output()
{
	if(_currentLine != nullptr)
	{
		_crt->output_data((unsigned int)((_writePointer - _currentLine) * _currentOutputDivider), _currentOutputDivider);
		_writePointer = _currentLine = nullptr;
	}
}

inline void Machine::update_pixels_to_position(int x, int y)
{
	while((display_x < x) || (display_y < y))
	{
		if(display_x < first_graphics_cycle)
		{
			display_x++;

			if(display_x == first_graphics_cycle)
			{
				_crt->output_sync(9 * crt_cycles_multiplier);
				_crt->output_blank((first_graphics_cycle - 9) * crt_cycles_multiplier);
				_currentScreenAddress = _startLineAddress;
			}
			continue;
		}

		if(display_x < last_graphics_cycle)
		{
			int cycles_to_output = (display_y < y) ? last_graphics_cycle - display_x : std::min(last_graphics_cycle - display_x, x - display_x);
			output_pixels(display_x, cycles_to_output);
			display_x += cycles_to_output;

			if(display_x == last_graphics_cycle)
			{
				end_pixel_output();
			}
			continue;
		}

		display_x++;
		if(display_x == 128)
		{
			_crt->output_blank((128 - 80 - first_graphics_cycle) * crt_cycles_multiplier);
			display_x = 0;
			display_y++;


			if(((_screen_mode != 3) && (_screen_mode != 6)) || ((display_y%10) < 8))
			{
				_startLineAddress++;
				_currentOutputLine++;

				if(_currentOutputLine == 8)
				{
					_currentOutputLine = 0;
					_startLineAddress = (_startLineAddress - 8) + ((_screen_mode < 4) ? 80 : 40)*8;
				}
			}
		}
	}
}

inline unsigned int Machine::get_first_graphics_cycle()
{
	return (first_graphics_line * cycles_per_line) - (_is_odd_field ? 0 : 64);
}

inline void Machine::update_display()
{
	/*

		Odd field:					Even field:

		|--S--|						   -S-|
		|--S--|						|--S--|
		|-S-B-|	= 3					|--S--| = 2.5
		|--B--|						|--B--|
		|--P--|						|--P--|
		|--B--| = 312				|--B--| = 312.5
		|-B-

	*/

	const unsigned int end_of_top = get_first_graphics_cycle();
	const unsigned int end_of_graphics = end_of_top + 256 * cycles_per_line;

	// does the top region need to be output?
	if(_displayOutputPosition < end_of_top && _fieldCycles >= end_of_top)
	{
		_crt->output_sync(320 * crt_cycles_multiplier);
		if(_is_odd_field) _crt->output_blank(64 * crt_cycles_multiplier);
		_displayOutputPosition += 320 + (_is_odd_field ? 64 : 0);

		while(_displayOutputPosition < end_of_top)
		{
			_displayOutputPosition += 128;
			_crt->output_sync(9 * crt_cycles_multiplier);
			_crt->output_blank(119 * crt_cycles_multiplier);
		}

		assert(_displayOutputPosition == end_of_top);

		reset_pixel_output();
	}

	// is this the pixel region?
	if(_displayOutputPosition >= end_of_top && _displayOutputPosition < end_of_graphics)
	{
		unsigned int final_position = std::min(_fieldCycles, end_of_graphics) - end_of_top;
		unsigned int final_line = final_position >> 7;
		unsigned int final_pixel = final_position & 127;
		update_pixels_to_position((int)final_pixel, (int)final_line);
		_displayOutputPosition = final_position + end_of_top;
	}

	// is this the bottom region?
	if(_displayOutputPosition >= end_of_graphics && _displayOutputPosition < cycles_per_frame)
	{
		unsigned int remaining_time = cycles_per_frame - _displayOutputPosition;
		while(remaining_time >= 128)
		{
			_crt->output_sync(9 * crt_cycles_multiplier);
			_crt->output_blank(119 * crt_cycles_multiplier);
			remaining_time -= 128;
		}

		if(remaining_time == 64)
		{
			_crt->output_sync(9 * crt_cycles_multiplier);
			_crt->output_blank(55 * crt_cycles_multiplier);
		}

		_displayOutputPosition = cycles_per_frame;

		_is_odd_field ^= true;
	}
}

void Machine::set_key_state(Key key, bool isPressed)
{
	if(key == KeyBreak)
	{
		set_reset_line(isPressed);
	}
	else
	{
		if(isPressed)
			_key_states[key >> 4] |= key&0xf;
		else
			_key_states[key >> 4] &= ~(key&0xf);
	}
}

/*
	Speaker
*/

void Speaker::get_samples(unsigned int number_of_samples, int16_t *target)
{
	if(!_is_enabled)
	{
		*target = 0;
	}
	else
	{
		*target = _output_level;
	}
	skip_samples(number_of_samples);
}

void Speaker::skip_samples(unsigned int number_of_samples)
{
	while(number_of_samples--)
	{
		_counter ++;
		if(_counter > _divider)
		{
			_counter = 0;
			_output_level ^= 8192;
		}
	}
}

void Speaker::set_divider(uint8_t divider)
{
	_divider = divider;
}

void Speaker::set_is_enabled(bool is_enabled)
{
	_is_enabled = is_enabled;
	_counter = 0;
}

/*
	Tape
*/

Tape::Tape() : _is_running(false), _data_register(0), _delegate(nullptr), _output_bits_remaining(0), _last_posted_interrupt_status(0), _interrupt_status(0) {}

void Tape::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape = tape;
	get_next_tape_pulse();
}

inline void Tape::get_next_tape_pulse()
{
	_time_into_pulse = 0;
	_current_pulse = _tape->get_next_pulse();
	if(_input_pulse_stepper == nullptr || _current_pulse.length.clock_rate != _input_pulse_stepper->get_output_rate())
	{
		_input_pulse_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper(_current_pulse.length.clock_rate, 2000000));
	}
}

inline void Tape::push_tape_bit(uint16_t bit)
{
	_data_register = (uint16_t)((_data_register >> 1) | (bit << 10));
	if(_bits_since_start)
	{
		_bits_since_start--;

		if(_bits_since_start == 7)
		{
			_interrupt_status &= ~Interrupt::ReceiveDataFull;
		}
	}
	evaluate_interrupts();
}

inline void Tape::reset_tape_input()
{
	_bits_since_start = 0;
//	_interrupt_status &= ~(Interrupt::ReceiveDataFull | Interrupt::TransmitDataEmpty | Interrupt::HighToneDetect);
//
//	if(_last_posted_interrupt_status != _interrupt_status)
//	{
//		_last_posted_interrupt_status = _interrupt_status;
//		if(_delegate) _delegate->tape_did_change_interrupt_status(this);
//	}
}

inline void Tape::evaluate_interrupts()
{
	if(!_bits_since_start)
	{
		if((_data_register&0x3) == 0x1)
		{
			_interrupt_status |= Interrupt::ReceiveDataFull;
			if(_is_in_input_mode) _bits_since_start = 9;
		}
	}

	if(_data_register == 0x3ff)
		_interrupt_status |= Interrupt::HighToneDetect;
	else
		_interrupt_status &= ~Interrupt::HighToneDetect;

	if(_last_posted_interrupt_status != _interrupt_status)
	{
		_last_posted_interrupt_status = _interrupt_status;
		if(_delegate) _delegate->tape_did_change_interrupt_status(this);
	}
}

inline void Tape::clear_interrupts(uint8_t interrupts)
{
	if(_interrupt_status & interrupts)
	{
		_interrupt_status &= ~interrupts;
		if(_delegate) _delegate->tape_did_change_interrupt_status(this);
	}
}

inline void Tape::set_is_in_input_mode(bool is_in_input_mode)
{
	_is_in_input_mode = is_in_input_mode;
}

inline void Tape::set_counter(uint8_t value)
{
	_output_pulse_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper(1200, 2000000));
}

inline void Tape::set_data_register(uint8_t value)
{
	_data_register = (uint16_t)((value << 2) | 1);
	printf("Loaded — %03x\n", _data_register);
	_bits_since_start = _output_bits_remaining = 9;
}

inline uint8_t Tape::get_data_register()
{
	return (uint8_t)(_data_register >> 2);
}

inline void Tape::run_for_cycles(unsigned int number_of_cycles)
{
	if(_is_enabled)
	{
		if(_is_in_input_mode)
		{
			if(_is_running && _tape != nullptr)
			{
				while(number_of_cycles--)
				{
					_time_into_pulse += (unsigned int)_input_pulse_stepper->step();
					if(_time_into_pulse == _current_pulse.length.length)
					{
						get_next_tape_pulse();

						_crossings[0] = _crossings[1];
						_crossings[1] = _crossings[2];
						_crossings[2] = _crossings[3];

						_crossings[3] = Tape::Unrecognised;
						if(_current_pulse.type != Storage::Tape::Pulse::Zero)
						{
							float pulse_length = (float)_current_pulse.length.length / (float)_current_pulse.length.clock_rate;
							if(pulse_length >= 0.35 / 2400.0 && pulse_length < 0.7 / 2400.0) _crossings[3] = Tape::Short;
							if(pulse_length >= 0.35 / 1200.0 && pulse_length < 0.7 / 1200.0) _crossings[3] = Tape::Long;
						}

						if(_crossings[0] == Tape::Long && _crossings[1] == Tape::Long)
						{
							push_tape_bit(0);
							_crossings[0] = _crossings[1] = Tape::Recognised;
						}
						else
						{
							if(_crossings[0] == Tape::Short && _crossings[1] == Tape::Short && _crossings[2] == Tape::Short && _crossings[3] == Tape::Short)
							{
								push_tape_bit(1);
								_crossings[0] = _crossings[1] =
								_crossings[2] = _crossings[3] = Tape::Recognised;
							}
						}

					}
				}
			}
		}
		else
		{
			while(number_of_cycles--)
			{
				if(_output_pulse_stepper->step())
				{
					_output_bits_remaining--;
					_bits_since_start--;
					if(!_output_bits_remaining)
					{
						_output_bits_remaining = 9;
						_interrupt_status |= Interrupt::TransmitDataEmpty;
					}

					evaluate_interrupts();

					_data_register = (_data_register >> 1) | 0x200;
					printf("Shifted — %03x\n", _data_register);
				}
			}
		}
	}
}
