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

using namespace Electron;

static const unsigned int cycles_per_line = 128;
static const unsigned int cycles_per_frame = 312*cycles_per_line + 64;
static const unsigned int crt_cycles_multiplier = 8;
static const unsigned int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

static const int first_graphics_line = 38;
static const int first_graphics_cycle = 33;

Machine::Machine() :
	_interruptControl(0),
	_frameCycles(0),
	_displayOutputPosition(0),
	_audioOutputPosition(0),
	_audioOutputPositionError(0),
	_currentOutputLine(0),
	_is_odd_field(false),
	_crt(std::unique_ptr<Outputs::CRT>(new Outputs::CRT(crt_cycles_per_line, Outputs::CRT::DisplayType::PAL50, 1, 1)))
{
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(vec2 coordinate)"
		"{"
			"float texValue = texture(texID, coordinate).r;"
			"return vec3(step(4.0/256.0, mod(texValue, 8.0/256.0)), step(2.0/256.0, mod(texValue, 4.0/256.0)), step(1.0/256.0, mod(texValue, 2.0/256.0)));"
		"}");
//	_crt->set_visible_area(Outputs::Rect(0.2f, 0.05f, 0.82f, 0.82f));

	memset(_keyStates, 0, sizeof(_keyStates));
	memset(_palette, 0xf, sizeof(_palette));
	_interruptStatus = 0x02;
	for(int c = 0; c < 16; c++)
		memset(_roms[c], 0xff, 16384);

	_speaker.set_input_rate(125000);
	_tape.set_delegate(this);
}

Machine::~Machine()
{
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
				(_frameCycles < first_graphics_line * cycles_per_line) ||
				(address < _startLineAddress && address < 0x3000)
			))
				update_display();

			_ram[address] = *value;
		}

		// for the entire frame, RAM is accessible only on odd cycles; in modes below 4
		// it's also accessible only outside of the pixel regions
		cycles += (_frameCycles&1)^1;
		if(_screenMode < 4)
		{
			const int current_line = _frameCycles >> 7;
			const int line_position = _frameCycles & 127;
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
				cycles += (_frameCycles&1)^1;
//				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

				switch(address&0xf)
				{
					case 0x0:
						if(isReadOperation(operation))
						{
							*value = _interruptStatus;
							_interruptStatus &= ~0x02;
						}
						else
						{
							_interruptControl = *value;
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
								if( interruptDisable&0x10 ) _interruptStatus &= ~Interrupt::DisplayEnd;
								if( interruptDisable&0x20 ) _interruptStatus &= ~Interrupt::RealTimeClock;
								if( interruptDisable&0x40 ) _interruptStatus &= ~Interrupt::HighToneDetect;
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
								if(((_activeRom&12) != 8) || (nextROM&8))
								{
									_activeRom = (Electron::ROMSlot)nextROM;
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
							if(new_screen_mode != _screenMode)
							{
								update_display();
								_screenMode = new_screen_mode;
								switch(_screenMode)
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
				switch(_activeRom)
				{
					case ROMSlotKeyboard:
					case ROMSlotKeyboard+1:
						*value = 0xf0;
						for(int address_line = 0; address_line < 14; address_line++)
						{
							if(!(address&(1 << address_line))) *value |= _keyStates[address_line];
						}
					break;
					default:
						*value = _roms[_activeRom][address & 16383];
					break;
				}
			}
		}
	}

//	if(operation == CPU6502::BusOperation::ReadOpcode)
//	{
//		printf("%04x: %02x (%d)\n", address, *value, _frameCycles);
//	}

	_frameCycles += cycles;
	switch(_frameCycles)
	{
		case 64*128:
		case 196*128:
			update_audio();
		break;

		case (first_graphics_line + 100)*128:
			update_audio();
			signal_interrupt(Interrupt::RealTimeClock);
		break;

		case (first_graphics_line + 256)*128:
			update_audio();
			signal_interrupt(Interrupt::DisplayEnd);
		break;

		case cycles_per_frame:
			update_display();
			update_audio();
			_frameCycles = 0;
			_displayOutputPosition = 0;
			_audioOutputPosition = 0;
			_currentOutputLine = 0;
		break;
	}

	_tape.run_for_cycles(cycles);

	return cycles;
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
	_interruptStatus |= interrupt;
	evaluate_interrupts();
}

void Machine::tape_did_change_interrupt_status(Tape *tape)
{
	_interruptStatus = (_interruptStatus & ~(Interrupt::TransmitDataEmpty | Interrupt::ReceiveDataFull | Interrupt::HighToneDetect)) | _tape.get_interrupt_status();
	evaluate_interrupts();
}

inline void Machine::evaluate_interrupts()
{
	if(_interruptStatus & _interruptControl)
	{
		_interruptStatus |= 1;
	}
	else
	{
		_interruptStatus &= ~1;
	}
	set_irq_line(_interruptStatus & 1);
}

inline void Machine::update_audio()
{
	int difference = _frameCycles - _audioOutputPosition;
	_audioOutputPosition = _frameCycles;
	_speaker.run_for_cycles((_audioOutputPositionError + difference) >> 4);
	_audioOutputPositionError = (_audioOutputPositionError + difference)&15;
}

inline void Machine::update_display()
{
	const int lines_of_hsync = 3;
	const int end_of_hsync = lines_of_hsync * cycles_per_line;

	if(_frameCycles >= end_of_hsync)
	{
		// assert sync for the first three lines of the display, with a break at the end for horizontal alignment
		if(_displayOutputPosition < end_of_hsync)
		{
			// on an odd field, output a half line of level data, then 2.5 lines of sync; on an even field
			// output 2.5 lines of sync, then half a line of level.
//			if (_is_odd_field)
//			{
				_crt->output_blank(64 * crt_cycles_multiplier);
				_crt->output_sync(320 * crt_cycles_multiplier);
//			}
//			else
//			{
//				_crt.output_sync(320 * crt_cycles_multiplier);
//				_crt.output_blank(64 * crt_cycles_multiplier);
//			}

			_is_odd_field ^= true;
			_displayOutputPosition = end_of_hsync;
		}

		while(_displayOutputPosition >= end_of_hsync && _displayOutputPosition < _frameCycles)
		{
			const int cycles_left = _frameCycles - _displayOutputPosition;

			const int fieldOutputPosition = _displayOutputPosition + (_is_odd_field ? 64 : 0);
			const int current_line = fieldOutputPosition >> 7;
			const int line_position = fieldOutputPosition & 127;

			// all lines then start with 9 cycles of sync
			if(line_position < 9)
			{
				int remaining_period = std::min(9 - line_position, cycles_left);
				_displayOutputPosition += remaining_period;

				if(line_position + remaining_period == 9)
				{
//					printf("!%d!", 9);
					_crt->output_sync(9 * crt_cycles_multiplier);
				}
			}
			else
			{
				bool isBlankLine =
					((_screenMode == 3) || (_screenMode == 6)) ?
						((current_line < first_graphics_line || current_line >= first_graphics_line+248) || (((current_line - first_graphics_line)%10) > 7)) :
						((current_line < first_graphics_line || current_line >= first_graphics_line+256));

				if(isBlankLine)
				{
					int remaining_period = std::min(128 - line_position, cycles_left);
					_crt->output_blank((unsigned int)remaining_period * crt_cycles_multiplier);
//					printf(".[%d]", remaining_period);
					_displayOutputPosition += remaining_period;
				}
				else
				{
					// there are then 15 cycles of blank, 80 cycles of pixels, and 24 further cycles of blank
					if(line_position < 24)
					{
						int remaining_period = std::min(24 - line_position, cycles_left);
						_crt->output_blank((unsigned int)remaining_period * crt_cycles_multiplier);
//						printf("/(%d)(%d)[%d]", 24 - line_position, cycles_left, remaining_period);
						_displayOutputPosition += remaining_period;

						if(line_position + remaining_period == 24)
						{
							switch(_screenMode)
							{
								case 0: case 3:				_currentOutputDivider = 1; break;
								case 1:	case 4: case 6:		_currentOutputDivider = 2; break;
								case 2: case 5:				_currentOutputDivider = 4; break;
							}

							_crt->allocate_write_area(80 * crt_cycles_multiplier / _currentOutputDivider);
							_currentLine = _writePointer = (uint8_t *)_crt->get_write_target_for_buffer(0);

							if(current_line == first_graphics_line)
								_startLineAddress = _startScreenAddress;
							_currentScreenAddress = _startLineAddress;
						}
					}

					if(line_position >= 24 && line_position < 104)
					{
						// determine whether the pixel clock divider has changed; if so write out the old
						// data and start a new run
						unsigned int newDivider = 0;
						switch(_screenMode)
						{
							case 0: case 3:				newDivider = 1; break;
							case 1:	case 4: case 6:		newDivider = 2; break;
							case 2: case 5:				newDivider = 4; break;
						}
						if(newDivider != _currentOutputDivider && _currentLine)
						{
							_crt->output_data((unsigned int)((_writePointer - _currentLine) * _currentOutputDivider * crt_cycles_multiplier), _currentOutputDivider);
							_currentOutputDivider = newDivider;
							_crt->allocate_write_area((size_t)((104 - (unsigned int)line_position) * crt_cycles_multiplier / _currentOutputDivider));
							_currentLine = _writePointer = (uint8_t *)_crt->get_write_target_for_buffer(0);
						}


						int pixels_to_output = std::min(104 - line_position, cycles_left);
						_displayOutputPosition += pixels_to_output;
//						printf("<- %d ->", pixels_to_output);
						if(_screenMode >= 4)
						{
							// just shifting wouldn't be enough if both
							if(_displayOutputPosition&1) pixels_to_output++;
							pixels_to_output >>= 1;
						}

#define GetNextPixels() \
	if(_currentScreenAddress&32768)\
	{\
		_currentScreenAddress = _screenModeBaseAddress + (_currentScreenAddress&32767);\
	}\
	uint8_t pixels = _ram[_currentScreenAddress];\
	_currentScreenAddress = _currentScreenAddress+8

						if(pixels_to_output && _writePointer)
						{
							switch(_screenMode)
							{
								default:
								case 0: case 3: case 4: case 6:
									while(pixels_to_output--)
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

								case 1:
								case 5:
									while(pixels_to_output--)
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
									while(pixels_to_output--)
									{
										GetNextPixels();
										_writePointer[0] = _palette[((pixels&0x80) >> 4) | ((pixels&0x20) >> 3) | ((pixels&0x08) >> 2) | ((pixels&0x02) >> 1)];
										_writePointer[1] = _palette[((pixels&0x40) >> 3) | ((pixels&0x10) >> 2) | ((pixels&0x04) >> 1) | ((pixels&0x01) >> 0)];
										_writePointer += 2;
									}
								break;
							}
						}
					}

#undef GetNextPixels

					if(line_position >= 104)
					{
						int pixels_to_output = std::min(128 - line_position, cycles_left);
						_crt->output_blank((unsigned int)pixels_to_output * crt_cycles_multiplier);
						_displayOutputPosition += pixels_to_output;

						if(line_position + pixels_to_output == 128)
						{
							_currentOutputLine++;
//							printf("\n%d: ", _currentOutputLine);
							if(!(_currentOutputLine&7))
							{
								_startLineAddress += ((_screenMode < 4) ? 80 : 40)*8 - 7;
							}
							else
								_startLineAddress++;

							if(_writePointer)
								_crt->output_data((unsigned int)((_writePointer - _currentLine) * _currentOutputDivider), _currentOutputDivider);
							else
								_crt->output_data(80 * crt_cycles_multiplier, _currentOutputDivider);
							_currentLine = nullptr;
							_writePointer = nullptr;
						}
					}
				}
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
			_keyStates[key >> 4] |= key&0xf;
		else
			_keyStates[key >> 4] &= ~(key&0xf);
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
	if(_pulse_stepper == nullptr || _current_pulse.length.clock_rate != _pulse_stepper->get_output_rate())
	{
		_pulse_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper(_current_pulse.length.clock_rate, 2000000));
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
	_pulse_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper(1200, 2000000));
}

inline void Tape::set_data_register(uint8_t value)
{
	_data_register = (uint16_t)((value << 2) | 1);
	_output_bits_remaining = 9;
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
					_time_into_pulse += (unsigned int)_pulse_stepper->step();
					if(_time_into_pulse == _current_pulse.length.length)
					{
						get_next_tape_pulse();

//						if(_crossings[0] != Tape::Recognised)
//						{
//							reset_tape_input();
//						}

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
				if(_pulse_stepper->step())
				{
					_output_bits_remaining--;
					if(!_output_bits_remaining)
					{
						_output_bits_remaining = 9;
						_interrupt_status |= Interrupt::TransmitDataEmpty;
					}

					evaluate_interrupts();

					_data_register = (_data_register >> 1) | 0x200;
				}
			}
		}
	}
}
