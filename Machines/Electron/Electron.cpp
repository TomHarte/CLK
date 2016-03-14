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

namespace {
	static const unsigned int cycles_per_line = 128;
	static const unsigned int lines_per_frame = 625;
	static const unsigned int cycles_per_frame = lines_per_frame * cycles_per_line;
	static const unsigned int crt_cycles_multiplier = 8;
	static const unsigned int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

	static const unsigned int field_divider_line = 312;	// i.e. the line, simultaneous with which, the first field's sync ends. So if
												// the first line with pixels in field 1 is the 20th in the frame, the first line
												// with pixels in field 2 will be 20+field_divider_line
	static const unsigned int first_graphics_line = 38;
	static const unsigned int first_graphics_cycle = 33;

	static const unsigned int real_time_clock_interrupt_line = 100;
	static const unsigned int display_end_interrupt_line = 256;
}

#define graphics_line(v)	((((v) >> 7) - first_graphics_line + field_divider_line) % field_divider_line)
#define graphics_column(v)	((((v) & 127) - first_graphics_cycle + 128) & 127)

Machine::Machine() :
	_interrupt_control(0),
	_interrupt_status(Interrupt::PowerOnReset),
	_frameCycles(0),
	_displayOutputPosition(0),
	_audioOutputPosition(0),
	_audioOutputPositionError(0),
	_current_pixel_line(-1),
	_use_fast_tape_hack(false),
	_crt(std::unique_ptr<Outputs::CRT::CRT>(new Outputs::CRT::CRT(crt_cycles_per_line, 8, Outputs::CRT::DisplayType::PAL50, 1, 1)))
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
//			if(!(
//				(_fieldCycles < first_graphics_line * cycles_per_line) ||
//				(address < _startLineAddress && address < 0x3000)
//			))
				update_display();

			_ram[address] = *value;
		}

		// for the entire frame, RAM is accessible only on odd cycles; in modes below 4
		// it's also accessible only outside of the pixel regions
		cycles += 1 + (_frameCycles&1);
		if(_screen_mode < 4)
		{
			update_display();
			const int current_line = graphics_line(_frameCycles + (_frameCycles&1));
			const int current_column = graphics_column(_frameCycles + (_frameCycles&1));
			if(current_line < 256 && current_column < 80 && !_isBlankLine)
				cycles += (unsigned int)(80 - current_column);
		}
	}
	else
	{
		if(address >= 0xc000)
		{
			if((address & 0xff00) == 0xfe00)
			{
				switch(address&0xf)
				{
					case 0x0:
						if(isReadOperation(operation))
						{
							*value = _interrupt_status;
							_interrupt_status &= ~PowerOnReset;
						}
						else
						{
							_interrupt_control = (*value) & ~1;
							evaluate_interrupts();
						}
					break;
					case 0x1:
					break;
					case 0x2:
						if(!isReadOperation(operation))
						{
							_startScreenAddress = (_startScreenAddress & 0xfe00) | (uint16_t)(((*value) & 0xe0) << 1);
							if(!_startScreenAddress) _startScreenAddress |= 0x8000;
						}
					break;
					case 0x3:
						if(!isReadOperation(operation))
						{
							_startScreenAddress = (_startScreenAddress & 0x01ff) | (uint16_t)(((*value) & 0x3f) << 9);
							if(!_startScreenAddress) _startScreenAddress |= 0x8000;
						}
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
//								printf("To mode %d, at %d cycles into field (%d)\n", new_screen_mode, _fieldCycles, _fieldCycles >> 7);
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
				{
					if(
						_use_fast_tape_hack &&
						(operation == CPU6502::BusOperation::ReadOpcode) &&
						(
							(address == 0xf4e5) || (address == 0xf4e6) ||	// double NOPs at 0xf4e5, 0xf6de, 0xf6fa and 0xfa51
							(address == 0xf6de) || (address == 0xf6df) ||	// act to disable the normal branch into tape-handling
							(address == 0xf6fa) || (address == 0xf6fb) ||	// code, forcing the OS along the serially-accessed ROM
							(address == 0xfa51) || (address == 0xfa52) ||	// pathway.

							(address == 0xf0a8)								// 0xf0a8 is from where a service call would normally be
																			// dispatched; we can check whether it would be call 14
																			// (i.e. read byte) and, if so, whether the OS was about to
																			// issue a read byte call to a ROM despite being the tape
																			// FS being selected. If so then this is a get byte that
																			// we should service synthetically. Put the byte into Y
																			// and set A to zero to report that action was taken, then
																			// allow the PC read to return an RTS.
						)
					)
					{
						uint8_t service_call = (uint8_t)get_value_of_register(CPU6502::Register::X);
						if(address == 0xf0a8)
						{
							if(!_ram[0x247] && service_call == 14)
							{
								_tape.set_delegate(nullptr);

								// TODO: handle tape wrap around.

								int cycles_left_while_plausibly_in_data = 50;
								_tape.clear_interrupts(Interrupt::ReceiveDataFull);
								while(1)
								{
									uint8_t tapeStatus = _tape.run_for_input_pulse();
									cycles_left_while_plausibly_in_data--;
									if(!cycles_left_while_plausibly_in_data) _fast_load_is_in_data = false;
									if(	(tapeStatus & Interrupt::ReceiveDataFull) &&
										(_fast_load_is_in_data || _tape.get_data_register() == 0x2a)
									) break;
								}
								_tape.set_delegate(this);
								_interrupt_status |= _tape.get_interrupt_status();

								_fast_load_is_in_data = true;
								set_value_of_register(CPU6502::Register::A, 0);
								set_value_of_register(CPU6502::Register::Y, _tape.get_data_register());
								*value = 0x60; // 0x60 is RTS
							}
							else
								*value = _os[address & 16383];
						}
						else
							*value = 0xea;
					}
					else
					{
						*value = _os[address & 16383];
					}
				}
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

	const unsigned int pixel_line_clock = _frameCycles;// + 128 - first_graphics_cycle + 80;
	const unsigned int line_before_cycle = graphics_line(pixel_line_clock);
	const unsigned int line_after_cycle = graphics_line(pixel_line_clock + cycles);

	// implicit assumption here: the number of 2Mhz cycles this bus operation will take
	// is never longer than a line. On the Electron, it's a safe one.
	if(line_before_cycle != line_after_cycle)
	{
		switch(line_before_cycle)
		{
			case real_time_clock_interrupt_line:	signal_interrupt(Interrupt::RealTimeClock);	break;
//			case real_time_clock_interrupt_line+1:	clear_interrupt(Interrupt::RealTimeClock);	break;
			case display_end_interrupt_line:		signal_interrupt(Interrupt::DisplayEnd);	break;
//			case display_end_interrupt_line+1:		clear_interrupt(Interrupt::DisplayEnd);		break;
		}
	}

	_frameCycles += cycles;

	// deal with frame wraparound by updating the two dependent subsystems
	// as though the exact end of frame had been hit, then reset those
	// and allow the frame cycle counter to assume its real value
	if(_frameCycles >= cycles_per_frame)
	{
		unsigned int nextFrameCycles = _frameCycles - cycles_per_frame;
		_frameCycles = cycles_per_frame;
		update_display();
		update_audio();
		_displayOutputPosition = 0;
		_audioOutputPosition = 0;
		_frameCycles = nextFrameCycles;
	}

	if(!(_frameCycles&31))
		update_audio();
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

inline void Machine::clear_interrupt(Electron::Interrupt interrupt)
{
	_interrupt_status &= ~interrupt;
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
	int difference = (int)_frameCycles - _audioOutputPosition;
	_audioOutputPosition = (int)_frameCycles;
	_speaker.run_for_cycles((_audioOutputPositionError + difference) >> 4);
	_audioOutputPositionError = (_audioOutputPositionError + difference)&15;
}

inline void Machine::start_pixel_line()
{
	_current_pixel_line = (_current_pixel_line+1)&255;
	if(!_current_pixel_line)
	{
		_startLineAddress = _startScreenAddress;
		_current_character_row = 0;
		_isBlankLine = false;
	}
	else
	{
		bool mode_has_blank_lines = (_screen_mode == 6) || (_screen_mode == 3);
		_isBlankLine = (mode_has_blank_lines && ((_current_character_row > 7 && _current_character_row < 10) || (_current_pixel_line > 249)));

		if(!_isBlankLine)
		{
			_startLineAddress++;

			if(_current_character_row > 7)
			{
				_startLineAddress += ((_screen_mode < 4) ? 80 : 40) * 8 - 8;
				_current_character_row = 0;
			}
		}
	}
	_currentScreenAddress = _startLineAddress;
	_current_pixel_column = 0;

	if(!_isBlankLine)
	{
		_crt->allocate_write_area(640);
		_currentLine = _crt->get_write_target_for_buffer(0);
	}
}

inline void Machine::end_pixel_line()
{
	if(!_isBlankLine) _crt->output_data(640, 1);
	_current_character_row++;
}

inline void Machine::output_pixels(unsigned int number_of_cycles)
{
	if(_isBlankLine)
	{
		_crt->output_blank(number_of_cycles * crt_cycles_multiplier);
	}
	else
	{
		while(number_of_cycles--)
		{
			if(!(_current_pixel_column&1) || _screen_mode < 4)
			{
				if(_currentScreenAddress&32768)
				{
					_currentScreenAddress = (_screenModeBaseAddress + _currentScreenAddress)&32767;
				}

				_last_pixel_byte = _ram[_currentScreenAddress];
				_currentScreenAddress = _currentScreenAddress+8;
			}

			switch(_screen_mode)
			{
				case 3:
				case 0:
				{
					_currentLine[0] = _palette[(_last_pixel_byte&0x80) >> 4];
					_currentLine[1] = _palette[(_last_pixel_byte&0x40) >> 3];
					_currentLine[2] = _palette[(_last_pixel_byte&0x20) >> 2];
					_currentLine[3] = _palette[(_last_pixel_byte&0x10) >> 1];
					_currentLine[4] = _palette[(_last_pixel_byte&0x08) >> 0];
					_currentLine[5] = _palette[(_last_pixel_byte&0x04) << 1];
					_currentLine[6] = _palette[(_last_pixel_byte&0x02) << 2];
					_currentLine[7] = _palette[(_last_pixel_byte&0x01) << 3];
				}
				break;

				case 1:
				{
					_currentLine[0] =
					_currentLine[1] = _palette[((_last_pixel_byte&0x80) >> 4) | ((_last_pixel_byte&0x08) >> 2)];
					_currentLine[2] =
					_currentLine[3] = _palette[((_last_pixel_byte&0x40) >> 3) | ((_last_pixel_byte&0x04) >> 1)];
					_currentLine[4] =
					_currentLine[5] = _palette[((_last_pixel_byte&0x20) >> 2) | ((_last_pixel_byte&0x02) >> 0)];
					_currentLine[6] =
					_currentLine[7] = _palette[((_last_pixel_byte&0x10) >> 1) | ((_last_pixel_byte&0x01) << 1)];
				}
				break;

				case 2:
				{
					_currentLine[0] =
					_currentLine[1] =
					_currentLine[2] =
					_currentLine[3] = _palette[((_last_pixel_byte&0x80) >> 4) | ((_last_pixel_byte&0x20) >> 3) | ((_last_pixel_byte&0x08) >> 2) | ((_last_pixel_byte&0x02) >> 1)];
					_currentLine[4] =
					_currentLine[5] =
					_currentLine[6] =
					_currentLine[7] = _palette[((_last_pixel_byte&0x40) >> 3) | ((_last_pixel_byte&0x10) >> 2) | ((_last_pixel_byte&0x04) >> 1) | ((_last_pixel_byte&0x01) >> 0)];
				}
				break;

				case 6:
				case 4:
				{
					if(_current_pixel_column&1)
					{
						_currentLine[0] =
						_currentLine[1] = _palette[(_last_pixel_byte&0x08) >> 0];
						_currentLine[2] =
						_currentLine[3] = _palette[(_last_pixel_byte&0x04) << 1];
						_currentLine[4] =
						_currentLine[5] = _palette[(_last_pixel_byte&0x02) << 2];
						_currentLine[6] =
						_currentLine[7] = _palette[(_last_pixel_byte&0x01) << 3];
					}
					else
					{
						_currentLine[0] =
						_currentLine[1] = _palette[(_last_pixel_byte&0x80) >> 4];
						_currentLine[2] =
						_currentLine[3] = _palette[(_last_pixel_byte&0x40) >> 3];
						_currentLine[4] =
						_currentLine[5] = _palette[(_last_pixel_byte&0x20) >> 2];
						_currentLine[6] =
						_currentLine[7] = _palette[(_last_pixel_byte&0x10) >> 1];
					}
				}
				break;

				case 5:
				{
					if(_current_pixel_column&1)
					{
						_currentLine[0] =
						_currentLine[1] =
						_currentLine[2] =
						_currentLine[3] = _palette[((_last_pixel_byte&0x20) >> 2) | ((_last_pixel_byte&0x02) >> 0)];
						_currentLine[4] =
						_currentLine[5] =
						_currentLine[6] =
						_currentLine[7] = _palette[((_last_pixel_byte&0x10) >> 1) | ((_last_pixel_byte&0x01) << 1)];
					}
					else
					{
						_currentLine[0] =
						_currentLine[1] =
						_currentLine[2] =
						_currentLine[3] = _palette[((_last_pixel_byte&0x80) >> 4) | ((_last_pixel_byte&0x08) >> 2)];
						_currentLine[4] =
						_currentLine[5] =
						_currentLine[6] =
						_currentLine[7] = _palette[((_last_pixel_byte&0x40) >> 3) | ((_last_pixel_byte&0x04) >> 1)];
					}
				}
				break;
			}

			_current_pixel_column++;
			_currentLine += 8;
		}
	}
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

	int final_line = _frameCycles >> 7;
	while(_displayOutputPosition < _frameCycles)
	{
		int line = _displayOutputPosition >> 7;

		// Priority one: sync.
		// ===================

		// full sync lines are 0, 1, field_divider_line+1 and field_divider_line+2
		if(line == 0 || line == 1 || line == field_divider_line+1 || line == field_divider_line+2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			_crt->output_sync(128 * crt_cycles_multiplier);
			_displayOutputPosition += 128;
			continue;
		}

		// line 2 is a left-sync line
		if(line == 2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			_crt->output_sync(64 * crt_cycles_multiplier);
			_crt->output_blank(64 * crt_cycles_multiplier);
			_displayOutputPosition += 128;
			continue;
		}

		// line field_divider_line is a right-sync line
		if(line == field_divider_line)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			_crt->output_blank(64 * crt_cycles_multiplier);
			_crt->output_sync(64 * crt_cycles_multiplier);
			_displayOutputPosition += 128;
			continue;
		}

		// Priority two: blank lines.
		// ==========================
		//
		// Given that it is not a sync line, this is a blank line if it is less than first_graphics_line, or greater
		// than first_graphics_line+255 and less than first_graphics_line+field_divider_line, or greater than
		// first_graphics_line+field_divider_line+255 (TODO: or this is Mode 3 or 6 and this should be blank)
		if(
			line < first_graphics_line ||
			(line > first_graphics_line+255 && line < first_graphics_line+field_divider_line) ||
			line > first_graphics_line+field_divider_line+255)
		{
			if(final_line == line) return;
			_crt->output_sync(9 * crt_cycles_multiplier);
			_crt->output_blank(119 * crt_cycles_multiplier);
			_displayOutputPosition += 128;
			continue;
		}

		// Final possibility: this is a pixel line.
		// ========================================

		// determine how far we're going from left to right
		unsigned int this_cycle = _displayOutputPosition&127;
		unsigned int final_cycle = _frameCycles&127;
		if(final_line > line)
		{
			final_cycle = 128;
		}

		// output format is:
		// 9 cycles: sync
		// ... to 24 cycles: colour burst
		// ... to first_graphics_cycle: blank
		// ... for 80 cycles: pixels
		// ... until end of line: blank
		while(this_cycle < final_cycle)
		{
			if(this_cycle < 9)
			{
				if(final_cycle < 9) return;
				_crt->output_sync(9 * crt_cycles_multiplier);
				_displayOutputPosition += 9;
				this_cycle = 9;
			}

			if(this_cycle < 24)
			{
				if(final_cycle < 24) return;
				_crt->output_colour_burst((24-9) * crt_cycles_multiplier, 0, 12);
				_displayOutputPosition += 24-9;
				this_cycle = 24;
				// TODO: phase shouldn't be zero on every line
			}

			if(this_cycle < first_graphics_cycle)
			{
				if(final_cycle < first_graphics_cycle) return;
				_crt->output_blank((first_graphics_cycle - 24) * crt_cycles_multiplier);
				_displayOutputPosition += first_graphics_cycle - 24;
				this_cycle = first_graphics_cycle;
				start_pixel_line();
			}

			if(this_cycle < first_graphics_cycle + 80)
			{
				unsigned int length_to_output = std::min(final_cycle, (first_graphics_cycle + 80)) - this_cycle;
				output_pixels(length_to_output);
				_displayOutputPosition += length_to_output;
				this_cycle += length_to_output;
			}

			if(this_cycle >= first_graphics_cycle + 80)
			{
				if(final_cycle < 128) return;
				end_pixel_line();
				_crt->output_blank((128 - (first_graphics_cycle + 80)) * crt_cycles_multiplier);
				_displayOutputPosition += 128 - (first_graphics_cycle + 80);
				this_cycle = 128;
			}
		}
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

Tape::Tape() :
	_is_running(false),
	_data_register(0),
	_delegate(nullptr),
	_output({.bits_remaining_until_empty = 0, .cycles_into_pulse = 0}),
	_last_posted_interrupt_status(0),
	_interrupt_status(0) {}

void Tape::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape = tape;
	get_next_tape_pulse();
}

inline void Tape::get_next_tape_pulse()
{
	_input.time_into_pulse = 0;
	_input.current_pulse = _tape->get_next_pulse();
	if(_input.pulse_stepper == nullptr || _input.current_pulse.length.clock_rate != _input.pulse_stepper->get_output_rate())
	{
		_input.pulse_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper(_input.current_pulse.length.clock_rate, 2000000));
	}
}

inline void Tape::push_tape_bit(uint16_t bit)
{
	_data_register = (uint16_t)((_data_register >> 1) | (bit << 10));

	if(_input.minimum_bits_until_full) _input.minimum_bits_until_full--;
	if(_input.minimum_bits_until_full == 8) _interrupt_status &= ~Interrupt::ReceiveDataFull;
	if(!_input.minimum_bits_until_full)
	{
		if((_data_register&0x3) == 0x1)
		{
			_interrupt_status |= Interrupt::ReceiveDataFull;
			if(_is_in_input_mode) _input.minimum_bits_until_full = 9;
		}
	}

	if(_output.bits_remaining_until_empty)	_output.bits_remaining_until_empty--;
	if(!_output.bits_remaining_until_empty)	_interrupt_status |= Interrupt::TransmitDataEmpty;

	if(_data_register == 0x3ff)	_interrupt_status |= Interrupt::HighToneDetect;
	else						_interrupt_status &= ~Interrupt::HighToneDetect;

	evaluate_interrupts();
}

inline void Tape::evaluate_interrupts()
{
	if(_last_posted_interrupt_status != _interrupt_status)
	{
		_last_posted_interrupt_status = _interrupt_status;
		if(_delegate) _delegate->tape_did_change_interrupt_status(this);
	}
}

inline void Tape::clear_interrupts(uint8_t interrupts)
{
	_interrupt_status &= ~interrupts;
	evaluate_interrupts();
}

inline void Tape::set_is_in_input_mode(bool is_in_input_mode)
{
	_is_in_input_mode = is_in_input_mode;
}

inline void Tape::set_counter(uint8_t value)
{
	_output.cycles_into_pulse = 0;
	_output.bits_remaining_until_empty = 0;
}

inline void Tape::set_data_register(uint8_t value)
{
	_data_register = (uint16_t)((value << 2) | 1);
	_output.bits_remaining_until_empty = 9;
}

inline uint8_t Tape::get_data_register()
{
	return (uint8_t)(_data_register >> 2);
}

inline uint8_t Tape::run_for_input_pulse()
{
	get_next_tape_pulse();

	_crossings[0] = _crossings[1];
	_crossings[1] = _crossings[2];
	_crossings[2] = _crossings[3];

	_crossings[3] = Tape::Unrecognised;
	if(_input.current_pulse.type != Storage::Tape::Pulse::Zero)
	{
		float pulse_length = (float)_input.current_pulse.length.length / (float)_input.current_pulse.length.clock_rate;
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

	return _interrupt_status;
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
					_input.time_into_pulse += (unsigned int)_input.pulse_stepper->step();
					if(_input.time_into_pulse == _input.current_pulse.length.length)
					{
						run_for_input_pulse();
					}
				}
			}
		}
		else
		{
			_output.cycles_into_pulse += number_of_cycles;
			while(_output.cycles_into_pulse > 1664)		// 1664 = the closest you can get to 1200 baud if you're looking for something
			{											// that divides the 125,000Hz clock that the sound divider runs off.
				_output.cycles_into_pulse -= 1664;
				push_tape_bit(1);
			}
		}
	}
}
