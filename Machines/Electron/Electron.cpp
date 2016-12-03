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
	static const unsigned int first_graphics_line = 31;
	static const unsigned int first_graphics_cycle = 33;

	static const unsigned int display_end_interrupt_line = 256;

	static const unsigned int real_time_clock_interrupt_1 = 16704;
	static const unsigned int real_time_clock_interrupt_2 = 56704;
}

#define graphics_line(v)	((((v) >> 7) - first_graphics_line + field_divider_line) % field_divider_line)
#define graphics_column(v)	((((v) & 127) - first_graphics_cycle + 128) & 127)

Machine::Machine() :
	interrupt_control_(0),
	interrupt_status_(Interrupt::PowerOnReset | Interrupt::TransmitDataEmpty | 0x80),
	frame_cycles_(0),
	display_output_position_(0),
	audio_output_position_(0),
	current_pixel_line_(-1),
	use_fast_tape_hack_(false),
	phase_(0)
{
	memset(key_states_, 0, sizeof(key_states_));
	memset(palette_, 0xf, sizeof(palette_));
	for(int c = 0; c < 16; c++)
		memset(roms_[c], 0xff, 16384);

	tape_.set_delegate(this);
	set_clock_rate(2000000);
}

void Machine::setup_output(float aspect_ratio)
{
	speaker_.reset(new Speaker);
	crt_.reset(new Outputs::CRT::CRT(crt_cycles_per_line, 8, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue >>= 4 - (int(icoordinate.x * 8) & 4);"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");

	// TODO: as implied below, I've introduced a clock's latency into the graphics pipeline somehow. Investigate.
	crt_->set_visible_area(crt_->get_rect_for_area(first_graphics_line - 3, 256, (first_graphics_cycle+1) * crt_cycles_multiplier, 80 * crt_cycles_multiplier, 4.0f / 3.0f));

	// The maximum output frequency is 62500Hz and all other permitted output frequencies are integral divisions of that;
	// however setting the speaker on or off can happen on any 2Mhz cycle, and probably (?) takes effect immediately. So
	// run the speaker at a 2000000Hz input rate, at least for the time being.
	speaker_->set_input_rate(2000000 / Speaker::clock_rate_divider);
}

void Machine::close_output()
{
	crt_ = nullptr;
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	unsigned int cycles = 1;

	if(address < 0x8000)
	{
		if(isReadOperation(operation))
		{
			*value = ram_[address];
		}
		else
		{
			if(
				(
					((frame_cycles_ >= first_graphics_line * cycles_per_line) && (frame_cycles_ < (first_graphics_line + 256) * cycles_per_line)) ||
					((frame_cycles_ >= (first_graphics_line + field_divider_line)  * cycles_per_line) && (frame_cycles_ < (first_graphics_line + 256 + field_divider_line) * cycles_per_line))
				)
			)
				update_display();

			ram_[address] = *value;
		}

		// for the entire frame, RAM is accessible only on odd cycles; in modes below 4
		// it's also accessible only outside of the pixel regions
		cycles += 1 + (frame_cycles_&1);
		if(screen_mode_ < 4)
		{
			const int current_line = graphics_line(frame_cycles_ + (frame_cycles_&1));
			const int current_column = graphics_column(frame_cycles_ + (frame_cycles_&1));
			if(current_line < 256 && current_column < 80 && !is_blank_line_)
				cycles += (unsigned int)(80 - current_column);
		}
	}
	else
	{
//		if((address >> 8) == 0xfc)
//		{
//			printf("d");
//		}
		switch(address & 0xff0f)
		{
			case 0xfe00:
				if(isReadOperation(operation))
				{
					*value = interrupt_status_;
					interrupt_status_ &= ~PowerOnReset;
				}
				else
				{
					interrupt_control_ = (*value) & ~1;
					evaluate_interrupts();
				}
			break;
			case 0xfe02:
				if(!isReadOperation(operation))
				{
					start_screen_address_ = (start_screen_address_ & 0xfe00) | (uint16_t)(((*value) & 0xe0) << 1);
					if(!start_screen_address_) start_screen_address_ |= 0x8000;
				}
			break;
			case 0xfe03:
				if(!isReadOperation(operation))
				{
					start_screen_address_ = (start_screen_address_ & 0x01ff) | (uint16_t)(((*value) & 0x3f) << 9);
					if(!start_screen_address_) start_screen_address_ |= 0x8000;
				}
			break;
			case 0xfe04:
				if(isReadOperation(operation))
				{
					*value = tape_.get_data_register();
					tape_.clear_interrupts(Interrupt::ReceiveDataFull);
				}
				else
				{
					tape_.set_data_register(*value);
					tape_.clear_interrupts(Interrupt::TransmitDataEmpty);
				}
			break;
			case 0xfe05:
				if(!isReadOperation(operation))
				{
					const uint8_t interruptDisable = (*value)&0xf0;
					if( interruptDisable )
					{
						if( interruptDisable&0x10 ) interrupt_status_ &= ~Interrupt::DisplayEnd;
						if( interruptDisable&0x20 ) interrupt_status_ &= ~Interrupt::RealTimeClock;
						if( interruptDisable&0x40 ) interrupt_status_ &= ~Interrupt::HighToneDetect;
						evaluate_interrupts();

						// TODO: NMI
					}

					// latch the paged ROM in case external hardware is being emulated
					active_rom_ = (Electron::ROMSlot)(*value & 0xf);

					// apply the ULA's test
					if(*value & 0x08)
					{
						if(*value & 0x04)
						{
							keyboard_is_active_ = false;
							basic_is_active_ = false;
						}
						else
						{
							keyboard_is_active_ = !(*value & 0x02);
							basic_is_active_ = !keyboard_is_active_;
						}
					}
				}
			break;
			case 0xfe06:
				if(!isReadOperation(operation))
				{
					update_audio();
					speaker_->set_divider(*value);
					tape_.set_counter(*value);
				}
			break;
			case 0xfe07:
				if(!isReadOperation(operation))
				{
					// update screen mode
					uint8_t new_screen_mode = ((*value) >> 3)&7;
					if(new_screen_mode == 7) new_screen_mode = 4;
					if(new_screen_mode != screen_mode_)
					{
						update_display();
						screen_mode_ = new_screen_mode;
						switch(screen_mode_)
						{
							case 0: case 1: case 2: screen_mode_base_address_ = 0x3000; break;
							case 3: screen_mode_base_address_ = 0x4000; break;
							case 4: case 5: screen_mode_base_address_ = 0x5800; break;
							case 6: screen_mode_base_address_ = 0x6000; break;
						}
					}

					// update speaker mode
					bool new_speaker_is_enabled = (*value & 6) == 2;
					if(new_speaker_is_enabled != speaker_is_enabled_)
					{
						update_audio();
						speaker_->set_is_enabled(new_speaker_is_enabled);
						speaker_is_enabled_ = new_speaker_is_enabled;
					}

					tape_.set_is_enabled((*value & 6) != 6);
					tape_.set_is_in_input_mode((*value & 6) == 0);
					tape_.set_is_running(((*value)&0x40) ? true : false);

					// TODO: caps lock LED
				}
			break;
			case 0xfe08: case 0xfe09: case 0xfe0a: case 0xfe0b: case 0xfe0c: case 0xfe0d: case 0xfe0e: case 0xfe0f:
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
						palette_[registers[index][0]]	= (palette_[registers[index][0]]&3)	| ((colour >> 1)&4);
						palette_[registers[index][1]]	= (palette_[registers[index][1]]&3)	| ((colour >> 0)&4);
						palette_[registers[index][2]]	= (palette_[registers[index][2]]&3)	| ((colour << 1)&4);
						palette_[registers[index][3]]	= (palette_[registers[index][3]]&3)	| ((colour << 2)&4);

						palette_[registers[index][2]]	= (palette_[registers[index][2]]&5)	| ((colour >> 4)&2);
						palette_[registers[index][3]]	= (palette_[registers[index][3]]&5)	| ((colour >> 3)&2);
					}
					else
					{
						palette_[registers[index][0]]	= (palette_[registers[index][0]]&6)	| ((colour >> 7)&1);
						palette_[registers[index][1]]	= (palette_[registers[index][1]]&6)	| ((colour >> 6)&1);
						palette_[registers[index][2]]	= (palette_[registers[index][2]]&6)	| ((colour >> 5)&1);
						palette_[registers[index][3]]	= (palette_[registers[index][3]]&6)	| ((colour >> 4)&1);

						palette_[registers[index][0]]	= (palette_[registers[index][0]]&5)	| ((colour >> 2)&2);
						palette_[registers[index][1]]	= (palette_[registers[index][1]]&5)	| ((colour >> 1)&2);
					}

					// regenerate all palette tables for now
#define pack(a, b) (uint8_t)((a << 4) | (b))
					for(int byte = 0; byte < 256; byte++)
					{
						uint8_t *target = (uint8_t *)&palette_tables_.forty1bpp[byte];
						target[0] = pack(palette_[(byte&0x80) >> 4], palette_[(byte&0x40) >> 3]);
						target[1] = pack(palette_[(byte&0x20) >> 2], palette_[(byte&0x10) >> 1]);

						target = (uint8_t *)&palette_tables_.eighty2bpp[byte];
						target[0] = pack(palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)], palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)]);
						target[1] = pack(palette_[((byte&0x20) >> 2) | ((byte&0x02) >> 0)], palette_[((byte&0x10) >> 1) | ((byte&0x01) << 1)]);

						target = (uint8_t *)&palette_tables_.eighty1bpp[byte];
						target[0] = pack(palette_[(byte&0x80) >> 4], palette_[(byte&0x40) >> 3]);
						target[1] = pack(palette_[(byte&0x20) >> 2], palette_[(byte&0x10) >> 1]);
						target[2] = pack(palette_[(byte&0x08) >> 0], palette_[(byte&0x04) << 1]);
						target[3] = pack(palette_[(byte&0x02) << 2], palette_[(byte&0x01) << 3]);

						palette_tables_.forty2bpp[byte] = pack(palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)], palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)]);
						palette_tables_.eighty4bpp[byte] = pack(	palette_[((byte&0x80) >> 4) | ((byte&0x20) >> 3) | ((byte&0x08) >> 2) | ((byte&0x02) >> 1)],
																palette_[((byte&0x40) >> 3) | ((byte&0x10) >> 2) | ((byte&0x04) >> 1) | ((byte&0x01) >> 0)]);
					}
#undef pack
				}
			}
			break;

			case 0xfc04: case 0xfc05: case 0xfc06: case 0xfc07:
				if(plus3_ && (address&0x00f0) == 0x00c0)
				{
					if(is_holding_shift_ && address == 0xfcc4)
					{
						is_holding_shift_ = false;
						set_key_state(KeyShift, false);
					}
					if(isReadOperation(operation))
						*value = plus3_->get_register(address);
					else
						plus3_->set_register(address, *value);
				}
			break;
			case 0xfc00:
				if(plus3_ && (address&0x00f0) == 0x00c0)
				{
					if(!isReadOperation(operation))
					{
						plus3_->set_control_register(*value);
					}
					else
						*value = 1;
				}
			break;

			default:
				if(address >= 0xc000)
				{
					if(isReadOperation(operation))
					{
						if(
							use_fast_tape_hack_ &&
							tape_.has_tape() &&
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
								if(!ram_[0x247] && service_call == 14)
								{
									tape_.set_delegate(nullptr);

									// TODO: handle tape wrap around.

									int cycles_left_while_plausibly_in_data = 50;
									tape_.clear_interrupts(Interrupt::ReceiveDataFull);
									while(!tape_.get_tape()->is_at_end())
									{
										tape_.run_for_input_pulse();
										cycles_left_while_plausibly_in_data--;
										if(!cycles_left_while_plausibly_in_data) fast_load_is_in_data_ = false;
										if(	(tape_.get_interrupt_status() & Interrupt::ReceiveDataFull) &&
											(fast_load_is_in_data_ || tape_.get_data_register() == 0x2a)
										) break;
									}
									tape_.set_delegate(this);
									tape_.clear_interrupts(Interrupt::ReceiveDataFull);
									interrupt_status_ |= tape_.get_interrupt_status();

									fast_load_is_in_data_ = true;
									set_value_of_register(CPU6502::Register::A, 0);
									set_value_of_register(CPU6502::Register::Y, tape_.get_data_register());
									*value = 0x60; // 0x60 is RTS
								}
								else
									*value = os_[address & 16383];
							}
							else
								*value = 0xea;
						}
						else
						{
							*value = os_[address & 16383];
						}
					}
				}
				else
				{
					if(isReadOperation(operation))
					{
						*value = roms_[active_rom_][address & 16383];
						if(keyboard_is_active_)
						{
							*value &= 0xf0;
							for(int address_line = 0; address_line < 14; address_line++)
							{
								if(!(address&(1 << address_line))) *value |= key_states_[address_line];
							}
						}
						if(basic_is_active_)
						{
							*value &= roms_[ROMSlotBASIC][address & 16383];
						}
					} else if(rom_write_masks_[active_rom_])
					{
						roms_[active_rom_][address & 16383] = *value;
					}
				}
			break;
		}
	}

//	if(operation == CPU6502::BusOperation::ReadOpcode)
//	{
//		printf("%04x: %02x (%d)\n", address, *value, _fieldCycles);
//	}

//	const int end_of_field =
//	if(frame_cycles_ < (256 + first_graphics_line) << 7))

	const unsigned int pixel_line_clock = frame_cycles_;// + 128 - first_graphics_cycle + 80;
	const unsigned int line_before_cycle = graphics_line(pixel_line_clock);
	const unsigned int line_after_cycle = graphics_line(pixel_line_clock + cycles);

	// implicit assumption here: the number of 2Mhz cycles this bus operation will take
	// is never longer than a line. On the Electron, it's a safe one.
	if(line_before_cycle != line_after_cycle)
	{
		switch(line_before_cycle)
		{
//			case real_time_clock_interrupt_line:	signal_interrupt(Interrupt::RealTimeClock);	break;
//			case real_time_clock_interrupt_line+1:	clear_interrupt(Interrupt::RealTimeClock);	break;
			case display_end_interrupt_line:		signal_interrupt(Interrupt::DisplayEnd);	break;
//			case display_end_interrupt_line+1:		clear_interrupt(Interrupt::DisplayEnd);		break;
		}
	}

	if(
		(pixel_line_clock < real_time_clock_interrupt_1 && pixel_line_clock + cycles >= real_time_clock_interrupt_1) ||
		(pixel_line_clock < real_time_clock_interrupt_2 && pixel_line_clock + cycles >= real_time_clock_interrupt_2))
	{
		signal_interrupt(Interrupt::RealTimeClock);
	}

	frame_cycles_ += cycles;

	if(!(frame_cycles_&127)) phase_ += 64;

	// deal with frame wraparound by updating the two dependent subsystems
	// as though the exact end of frame had been hit, then reset those
	// and allow the frame cycle counter to assume its real value
	if(frame_cycles_ >= cycles_per_frame)
	{
		unsigned int nextFrameCycles = frame_cycles_ - cycles_per_frame;
		frame_cycles_ = cycles_per_frame;
		update_display();
		update_audio();
		display_output_position_ = 0;
		audio_output_position_ = 0;
		frame_cycles_ = nextFrameCycles;
	}

	if(!(frame_cycles_&16383))
		update_audio();
	tape_.run_for_cycles(cycles);

	if(typer_) typer_->update((int)cycles);
	if(plus3_) plus3_->run_for_cycles(4*cycles);

	return cycles;
}

void Machine::synchronise()
{
	update_display();
	update_audio();
	speaker_->flush();
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(target.tapes.size())
	{
		tape_.set_tape(target.tapes.front());
	}

	if(target.disks.size())
	{
		plus3_.reset(new Plus3);

		if(target.acorn.has_dfs)
		{
			set_rom(ROMSlot0, dfs_, true);
		}
		if(target.acorn.has_adfs)
		{
			set_rom(ROMSlot4, adfs_, true);
			set_rom(ROMSlot5, std::vector<uint8_t>(adfs_.begin() + 16384, adfs_.end()), true);
		}

		plus3_->set_disk(target.disks.front(), 0);
	}

	ROMSlot slot = ROMSlot12;
	for(std::shared_ptr<Storage::Cartridge::Cartridge> cartridge : target.cartridges)
	{
		set_rom(slot, cartridge->get_segments().front().data, false);
		slot = (ROMSlot)(((int)slot + 1)&15);
	}

	if(target.loadingCommand.length())	// TODO: and automatic loading option enabled
	{
		set_typer_for_string(target.loadingCommand.c_str());
	}
	if(target.acorn.should_hold_shift)
	{
		set_key_state(KeyShift, true);
		is_holding_shift_ = true;
	}
}

void Machine::set_rom(ROMSlot slot, std::vector<uint8_t> data, bool is_writeable)
{
	uint8_t *target = nullptr;
	switch(slot)
	{
		case ROMSlotDFS:	dfs_ = data;			return;
		case ROMSlotADFS:	adfs_ = data;			return;

		case ROMSlotOS:		target = os_;			break;
		default:
			target = roms_[slot];
			rom_write_masks_[slot] = is_writeable;
		break;
	}

	memcpy(target, &data[0], std::min((size_t)16384, data.size()));
}

inline void Machine::signal_interrupt(Electron::Interrupt interrupt)
{
	interrupt_status_ |= interrupt;
	evaluate_interrupts();
}

inline void Machine::clear_interrupt(Electron::Interrupt interrupt)
{
	interrupt_status_ &= ~interrupt;
	evaluate_interrupts();
}

void Machine::tape_did_change_interrupt_status(Tape *tape)
{
	interrupt_status_ = (interrupt_status_ & ~(Interrupt::TransmitDataEmpty | Interrupt::ReceiveDataFull | Interrupt::HighToneDetect)) | tape_.get_interrupt_status();
	evaluate_interrupts();
}

inline void Machine::evaluate_interrupts()
{
	if(interrupt_status_ & interrupt_control_)
	{
		interrupt_status_ |= 1;
	}
	else
	{
		interrupt_status_ &= ~1;
	}
	set_irq_line(interrupt_status_ & 1);
}

inline void Machine::update_audio()
{
	unsigned int difference = frame_cycles_ - audio_output_position_ + audio_output_position_error_;
	audio_output_position_ = frame_cycles_;
	speaker_->run_for_cycles(difference / Speaker::clock_rate_divider);
	audio_output_position_error_ = difference % Speaker::clock_rate_divider;
}

inline void Machine::start_pixel_line()
{
	current_pixel_line_ = (current_pixel_line_+1)&255;
	if(!current_pixel_line_)
	{
		start_line_address_ = start_screen_address_;
		current_character_row_ = 0;
		is_blank_line_ = false;
	}
	else
	{
		bool mode_has_blank_lines = (screen_mode_ == 6) || (screen_mode_ == 3);
		is_blank_line_ = (mode_has_blank_lines && ((current_character_row_ > 7 && current_character_row_ < 10) || (current_pixel_line_ > 249)));

		if(!is_blank_line_)
		{
			start_line_address_++;

			if(current_character_row_ > 7)
			{
				start_line_address_ += ((screen_mode_ < 4) ? 80 : 40) * 8 - 8;
				current_character_row_ = 0;
			}
		}
	}
	current_screen_address_ = start_line_address_;
	current_pixel_column_ = 0;
	initial_output_target_ = current_output_target_ = nullptr;
}

inline void Machine::end_pixel_line()
{
	if(current_output_target_) crt_->output_data((unsigned int)((current_output_target_ - initial_output_target_) * current_output_divider_), current_output_divider_);
	current_character_row_++;
}

inline void Machine::output_pixels(unsigned int number_of_cycles)
{
	if(!number_of_cycles) return;

	if(is_blank_line_)
	{
		crt_->output_blank(number_of_cycles * crt_cycles_multiplier);
	}
	else
	{
		unsigned int divider = 0;
		switch(screen_mode_)
		{
			case 0: case 3: divider = 2; break;
			case 1: case 4: case 6: divider = 4; break;
			case 2: case 5: divider = 8; break;
		}

		if(!initial_output_target_ || divider != current_output_divider_)
		{
			if(current_output_target_) crt_->output_data((unsigned int)((current_output_target_ - initial_output_target_) * current_output_divider_), current_output_divider_);
			current_output_divider_ = divider;
			initial_output_target_ = current_output_target_ = crt_->allocate_write_area(640 / current_output_divider_);
		}

#define get_pixel()	\
				if(current_screen_address_&32768)\
				{\
					current_screen_address_ = (screen_mode_base_address_ + current_screen_address_)&32767;\
				}\
				last_pixel_byte_ = ram_[current_screen_address_];\
				current_screen_address_ = current_screen_address_+8

		switch(screen_mode_)
		{
			case 0: case 3:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*(uint32_t *)current_output_target_ = palette_tables_.eighty1bpp[last_pixel_byte_];
						current_output_target_ += 4;
						current_pixel_column_++;
					}
				} else current_output_target_ += 4*number_of_cycles;
			break;

			case 1:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.eighty2bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2*number_of_cycles;
			break;

			case 2:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.eighty4bpp[last_pixel_byte_];
						current_output_target_ += 1;
						current_pixel_column_++;
					}
				} else current_output_target_ += number_of_cycles;
			break;

			case 4: case 6:
				if(initial_output_target_)
				{
					if(current_pixel_column_&1)
					{
						last_pixel_byte_ <<= 4;
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						last_pixel_byte_ <<= 4;
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2 * number_of_cycles;
			break;

			case 5:
				if(initial_output_target_)
				{
					if(current_pixel_column_&1)
					{
						last_pixel_byte_ <<= 2;
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						last_pixel_byte_ <<= 2;
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;
						current_pixel_column_++;
					}
				} else current_output_target_ += number_of_cycles;
			break;
		}

#undef get_pixel
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

	int final_line = frame_cycles_ >> 7;
	while(display_output_position_ < frame_cycles_)
	{
		int line = display_output_position_ >> 7;

		// Priority one: sync.
		// ===================

		// full sync lines are 0, 1, field_divider_line+1 and field_divider_line+2
		if(line == 0 || line == 1 || line == field_divider_line+1 || line == field_divider_line+2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(128 * crt_cycles_multiplier);
			display_output_position_ += 128;
			continue;
		}

		// line 2 is a left-sync line
		if(line == 2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(64 * crt_cycles_multiplier);
			crt_->output_blank(64 * crt_cycles_multiplier);
			display_output_position_ += 128;
			continue;
		}

		// line field_divider_line is a right-sync line
		if(line == field_divider_line)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(9 * crt_cycles_multiplier);
			crt_->output_blank(55 * crt_cycles_multiplier);
			crt_->output_sync(64 * crt_cycles_multiplier);
			display_output_position_ += 128;
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
			crt_->output_sync(9 * crt_cycles_multiplier);
			crt_->output_blank(119 * crt_cycles_multiplier);
			display_output_position_ += 128;
			continue;
		}

		// Final possibility: this is a pixel line.
		// ========================================

		// determine how far we're going from left to right
		unsigned int this_cycle = display_output_position_&127;
		unsigned int final_cycle = frame_cycles_&127;
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
				crt_->output_sync(9 * crt_cycles_multiplier);
				display_output_position_ += 9;
				this_cycle = 9;
			}

			if(this_cycle < 24)
			{
				if(final_cycle < 24) return;
				crt_->output_colour_burst((24-9) * crt_cycles_multiplier, phase_, 12);
				display_output_position_ += 24-9;
				this_cycle = 24;
				// TODO: phase shouldn't be zero on every line
			}

			if(this_cycle < first_graphics_cycle)
			{
				if(final_cycle < first_graphics_cycle) return;
				crt_->output_blank((first_graphics_cycle - 24) * crt_cycles_multiplier);
				display_output_position_ += first_graphics_cycle - 24;
				this_cycle = first_graphics_cycle;
				start_pixel_line();
			}

			if(this_cycle < first_graphics_cycle + 80)
			{
				unsigned int length_to_output = std::min(final_cycle, (first_graphics_cycle + 80)) - this_cycle;
				output_pixels(length_to_output);
				display_output_position_ += length_to_output;
				this_cycle += length_to_output;
			}

			if(this_cycle >= first_graphics_cycle + 80)
			{
				if(final_cycle < 128) return;
				end_pixel_line();
				crt_->output_blank((128 - (first_graphics_cycle + 80)) * crt_cycles_multiplier);
				display_output_position_ += 128 - (first_graphics_cycle + 80);
				this_cycle = 128;
			}
		}
	}
}

void Machine::clear_all_keys()
{
	memset(key_states_, 0, sizeof(key_states_));
}

void Machine::set_key_state(uint16_t key, bool isPressed)
{
	if(key == KeyBreak)
	{
		set_reset_line(isPressed);
	}
	else
	{
		if(isPressed)
			key_states_[key >> 4] |= key&0xf;
		else
			key_states_[key >> 4] &= ~(key&0xf);
	}
}
