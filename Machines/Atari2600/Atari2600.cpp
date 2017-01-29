//
//  Atari2600.cpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "Atari2600.hpp"
#include <algorithm>
#include <stdio.h>

using namespace Atari2600;
namespace {
	static const unsigned int horizontalTimerPeriod = 228;
	static const double NTSC_clock_rate = 1194720;
	static const double PAL_clock_rate = 1182298;
}

Machine::Machine() :
	rom_(nullptr),
	rom_pages_{nullptr, nullptr, nullptr, nullptr},
	tia_input_value_{0xff, 0xff},
	cycles_since_speaker_update_(0),
	cycles_since_video_update_(0)
{
	set_clock_rate(NTSC_clock_rate);
}

void Machine::setup_output(float aspect_ratio)
{
	tia_.reset(new TIA);
	speaker_.reset(new Speaker);
}

void Machine::close_output()
{
	tia_ = nullptr;
	speaker_ = nullptr;
}

Machine::~Machine()
{
	delete[] rom_;
	close_output();
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 3;

	// this occurs as a feedback loop — the 2600 requests ready, then performs the cycles_run_for
	// leap to the end of ready only once ready is signalled — because on a 6502 ready doesn't take
	// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
	// skips to the end of the line.
	if(operation == CPU6502::BusOperation::Ready) {
		cycles_run_for = (unsigned int)tia_->get_cycles_until_horizontal_blank(cycles_since_video_update_);
	}

	cycles_since_speaker_update_ += cycles_run_for;
	cycles_since_video_update_ += cycles_run_for;

	if(operation != CPU6502::BusOperation::Ready) {

		// check for a paging access
		if(rom_size_ > 4096 && ((address & 0x1f00) == 0x1f00)) {
			uint8_t *base_ptr = rom_pages_[0];
			uint8_t first_paging_register = (uint8_t)(0xf8 - (rom_size_ >> 14)*2);

			const uint8_t paging_register = address&0xff;
			if(paging_register >= first_paging_register) {
				const uint16_t selected_page = paging_register - first_paging_register;
				if(selected_page * 4096 < rom_size_) {
					base_ptr = &rom_[selected_page * 4096];
				}
			}

			if(base_ptr != rom_pages_[0]) {
				rom_pages_[0] = base_ptr;
				rom_pages_[1] = base_ptr + 1024;
				rom_pages_[2] = base_ptr + 2048;
				rom_pages_[3] = base_ptr + 3072;
			}
		}

		// check for a ROM read
		if((address&0x1000) && isReadOperation(operation)) {
			returnValue &= rom_pages_[(address >> 10)&3][address&1023];
		}

		// check for a RAM access
		if((address&0x1280) == 0x80) {
			if(isReadOperation(operation)) {
				returnValue &= mos6532_.get_ram(address);
			} else {
				mos6532_.set_ram(address, *value);
			}
		}

		// check for a TIA access
		if(!(address&0x1080)) {
			if(isReadOperation(operation)) {
				const uint16_t decodedAddress = address & 0xf;
				switch(decodedAddress) {
					case 0x00:		// missile 0 / player collisions
					case 0x01:		// missile 1 / player collisions
					case 0x02:		// player 0 / playfield / ball collisions
					case 0x03:		// player 1 / playfield / ball collisions
					case 0x04:		// missile 0 / playfield / ball collisions
					case 0x05:		// missile 1 / playfield / ball collisions
					case 0x06:		// ball / playfield collisions
					case 0x07:		// player / player, missile / missile collisions
						returnValue &= tia_->get_collision_flags(decodedAddress);
					break;

					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
						// TODO: pot ports
					break;

					case 0x0c:
					case 0x0d:
						returnValue &= tia_input_value_[decodedAddress - 0x0c];
					break;
				}
			} else {
				const uint16_t decodedAddress = address & 0x3f;
				switch(decodedAddress) {
					case 0x00:	update_video(); tia_->set_vsync(!!(*value & 0x02));		break;
					case 0x01:	update_video();	tia_->set_vblank(!!(*value & 0x02));	break;

					case 0x02:
						set_ready_line(true);
						// TODO: if(horizontal_timer_)
					break;
					case 0x03:	update_video();	tia_->reset_horizontal_counter();		break;
						// TODO: audio will now be out of synchronisation — fix

					case 0x04:
					case 0x05:	update_video();	tia_->set_player_number_and_size(decodedAddress - 0x04, *value);	break;
					case 0x06:
					case 0x07:	update_video();	tia_->set_player_missile_colour(decodedAddress - 0x06, *value);		break;
					case 0x08:	update_video();	tia_->set_playfield_ball_colour(*value);							break;
					case 0x09:	update_video();	tia_->set_background_colour(*value);								break;
					case 0x0a:	update_video();	tia_->set_playfield_control_and_ball_size(*value);					break;
					case 0x0b:
					case 0x0c:	update_video();	tia_->set_player_reflected(decodedAddress - 0x0b, !((*value)&8));	break;
					case 0x0d:
					case 0x0e:
					case 0x0f:	update_video();	tia_->set_playfield(decodedAddress - 0x0d, *value);					break;
					case 0x10:
					case 0x11:	update_video(); tia_->set_player_position(decodedAddress - 0x10);					break;
					case 0x12:
					case 0x13:	update_video(); tia_->set_missile_position(decodedAddress - 0x13);					break;
					case 0x14:	update_video();	tia_->set_ball_position();											break;
					case 0x1b:
					case 0x1c:	update_video(); tia_->set_player_graphic(decodedAddress - 0x1b, *value);			break;
					case 0x1d:
					case 0x1e:	update_video(); tia_->set_missile_enable(decodedAddress - 0x1d, !!((*value)&2));	break;
					case 0x1f:	update_video(); tia_->set_ball_enable(!!((*value)&2));								break;
					case 0x20:
					case 0x21:	update_video(); tia_->set_player_motion(decodedAddress - 0x20, *value);				break;
					case 0x22:
					case 0x23:	update_video(); tia_->set_missile_motion(decodedAddress - 0x22, *value);			break;
					case 0x24:	update_video(); tia_->set_ball_motion(*value);										break;
					case 0x25:
					case 0x26:	tia_->set_player_delay(decodedAddress - 0x25, !!((*value)&1));						break;
					case 0x27:	tia_->set_ball_delay(!!((*value)&1));												break;
					case 0x28:
					case 0x29:	update_video(); tia_->set_missile_position_to_player(decodedAddress - 0x28);		break;
					case 0x2a:	update_video(); tia_->move();														break;
					case 0x2b:	update_video(); tia_->clear_motion();												break;
					case 0x2c:	update_video(); tia_->clear_collision_flags();										break;

					case 0x15:
					case 0x16:	update_audio(); speaker_->set_control(decodedAddress - 0x15, *value);				break;
					case 0x17:
					case 0x18:	update_audio(); speaker_->set_divider(decodedAddress - 0x17, *value);				break;
					case 0x19:
					case 0x1a:	update_audio(); speaker_->set_volume(decodedAddress - 0x19, *value);				break;
				}
			}
		}

		// check for a PIA access
		if((address&0x1280) == 0x280) {
			if(isReadOperation(operation)) {
				returnValue &= mos6532_.get_register(address);
			} else {
				mos6532_.set_register(address, *value);
			}
		}

		if(isReadOperation(operation)) {
			*value = returnValue;
		}
	}

	mos6532_.run_for_cycles(cycles_run_for / 3);

	return cycles_run_for / 3;
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state)
{
	switch (input) {
		case Atari2600DigitalInputJoy1Up:		mos6532_.update_port_input(0, 0x10, state);	break;
		case Atari2600DigitalInputJoy1Down:		mos6532_.update_port_input(0, 0x20, state);	break;
		case Atari2600DigitalInputJoy1Left:		mos6532_.update_port_input(0, 0x40, state);	break;
		case Atari2600DigitalInputJoy1Right:	mos6532_.update_port_input(0, 0x80, state);	break;

		case Atari2600DigitalInputJoy2Up:		mos6532_.update_port_input(0, 0x01, state);	break;
		case Atari2600DigitalInputJoy2Down:		mos6532_.update_port_input(0, 0x02, state);	break;
		case Atari2600DigitalInputJoy2Left:		mos6532_.update_port_input(0, 0x04, state);	break;
		case Atari2600DigitalInputJoy2Right:	mos6532_.update_port_input(0, 0x08, state);	break;

		// TODO: latching
		case Atari2600DigitalInputJoy1Fire:		if(state) tia_input_value_[0] &= ~0x80; else tia_input_value_[0] |= 0x80; break;
		case Atari2600DigitalInputJoy2Fire:		if(state) tia_input_value_[1] &= ~0x80; else tia_input_value_[1] |= 0x80; break;

		default: break;
	}
}

void Machine::set_switch_is_enabled(Atari2600Switch input, bool state)
{
	switch(input) {
		case Atari2600SwitchReset:					mos6532_.update_port_input(1, 0x01, state);	break;
		case Atari2600SwitchSelect:					mos6532_.update_port_input(1, 0x02, state);	break;
		case Atari2600SwitchColour:					mos6532_.update_port_input(1, 0x08, state);	break;
		case Atari2600SwitchLeftPlayerDifficulty:	mos6532_.update_port_input(1, 0x40, state);	break;
		case Atari2600SwitchRightPlayerDifficulty:	mos6532_.update_port_input(1, 0x80, state);	break;
	}
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(!target.cartridges.front()->get_segments().size()) return;
	Storage::Cartridge::Cartridge::Segment segment = target.cartridges.front()->get_segments().front();
	size_t length = segment.data.size();

	rom_size_ = 1024;
	while(rom_size_ < length && rom_size_ < 32768) rom_size_ <<= 1;

	delete[] rom_;
	rom_ = new uint8_t[rom_size_];

	size_t offset = 0;
	const size_t copy_step = std::min(rom_size_, length);
	while(offset < rom_size_)
	{
		size_t copy_length = std::min(copy_step, rom_size_ - offset);
		memcpy(&rom_[offset], &segment.data[0], copy_length);
		offset += copy_length;
	}

	size_t romMask = rom_size_ - 1;
	rom_pages_[0] = rom_;
	rom_pages_[1] = &rom_[1024 & romMask];
	rom_pages_[2] = &rom_[2048 & romMask];
	rom_pages_[3] = &rom_[3072 & romMask];
}

#pragma mark - Audio and Video

void Machine::update_audio()
{
	unsigned int audio_cycles = cycles_since_speaker_update_ / 114;

	speaker_->run_for_cycles(audio_cycles);
	cycles_since_speaker_update_ %= 114;
}

void Machine::update_video()
{
	tia_->run_for_cycles((int)cycles_since_video_update_);
	cycles_since_video_update_ = 0;
}

void Machine::synchronise()
{
	update_audio();
	update_video();
	speaker_->flush();
}
