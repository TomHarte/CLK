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
	static const double NTSC_clock_rate = 1194720;
	static const double PAL_clock_rate = 1182298;
}

Machine::Machine() :
	rom_(nullptr),
	rom_pages_{nullptr, nullptr, nullptr, nullptr},
	tia_input_value_{0xff, 0xff},
	cycles_since_speaker_update_(0),
	cycles_since_video_update_(0),
	cycles_since_6532_update_(0),
	frame_record_pointer_(0),
	is_ntsc_(true) {
	set_clock_rate(NTSC_clock_rate);
}

void Machine::setup_output(float aspect_ratio) {
	tia_.reset(new TIA);
	speaker_.reset(new Speaker);
	speaker_->set_input_rate((float)(get_clock_rate() / 38.0));
	tia_->get_crt()->set_delegate(this);
}

void Machine::close_output() {
	tia_ = nullptr;
	speaker_ = nullptr;
}

Machine::~Machine() {
	delete[] rom_;
	close_output();
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value) {
	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 3;

	// this occurs as a feedback loop — the 2600 requests ready, then performs the cycles_run_for
	// leap to the end of ready only once ready is signalled — because on a 6502 ready doesn't take
	// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
	// skips to the end of the line.
	if(operation == CPU6502::BusOperation::Ready)
		cycles_run_for = (unsigned int)tia_->get_cycles_until_horizontal_blank(cycles_since_video_update_);

	cycles_since_speaker_update_ += cycles_run_for;
	cycles_since_video_update_ += cycles_run_for;
	cycles_since_6532_update_ += (cycles_run_for / 3);

	if(operation != CPU6502::BusOperation::Ready) {
		uint16_t masked_address = address & 0x1fff;

#define AtariPager(start, end) \
	if(masked_address >= start && masked_address <= end) {	\
		uint8_t *base_ptr = &rom_[(masked_address - start) * 4096];\
		if(base_ptr != rom_pages_[0]) {\
			rom_pages_[0] = base_ptr;\
			rom_pages_[1] = base_ptr + 1024;\
			rom_pages_[2] = base_ptr + 2048;\
			rom_pages_[3] = base_ptr + 3072;\
		}\
	}

		// check for potential paging
		switch(paging_model_) {
			default:
			break;
			case StaticAnalyser::Atari2600PagingModel::Atari8k:		AtariPager(0x1ff8, 0x1ff9);	break;
			case StaticAnalyser::Atari2600PagingModel::CBSRamPlus:	AtariPager(0x1ff8, 0x1ffa);	break;
			case StaticAnalyser::Atari2600PagingModel::Atari16k:	AtariPager(0x1ff6, 0x1ff9);	break;
			case StaticAnalyser::Atari2600PagingModel::Atari32k:	AtariPager(0x1ff4, 0x1ffb);	break;
			case StaticAnalyser::Atari2600PagingModel::ParkerBros:
				if(masked_address >= 0x1fe0 && masked_address < 0x1ff8) {
					int slot = (masked_address >> 3) & 3;
					int target = masked_address & 7;
					rom_pages_[slot] = &rom_[target * 1024];
				}
			break;
			case StaticAnalyser::Atari2600PagingModel::MegaBoy:
				if(masked_address == 0x1fec && isReadOperation(operation)) {
					*value = mega_boy_page_;
				}
				if(masked_address == 0x1ff0) {
					mega_boy_page_ = (mega_boy_page_ + 1) & 15;
					rom_pages_[0] = &rom_[mega_boy_page_ * 4096];
					rom_pages_[1] = rom_pages_[0] + 1024;
					rom_pages_[2] = rom_pages_[0] + 2048;
					rom_pages_[3] = rom_pages_[0] + 3072;
				}
			break;
			case StaticAnalyser::Atari2600PagingModel::MNetwork:
				if(masked_address >= 0x1fe0 && masked_address < 0x1fe7) {
					int target = (masked_address & 7) * 2048;
					rom_pages_[0] = &rom_[target];
					rom_pages_[1] = &rom_[target] + 1024;
				} else if(masked_address == 0x1fe7) {
					for(int c = 0; c < 8; c++) {
						ram_write_targets_[c] = ram_.data() + 1024 + c * 128;
						ram_read_targets_[c + 8] = ram_write_targets_[c];
					}
				} else if(masked_address >= 0x1fe8 && masked_address <= 0x1ffb) {
					int offset = (masked_address - 0x1fe8) * 256;
					ram_write_targets_[16] = ram_.data() + offset;
					ram_write_targets_[17] = ram_write_targets_[16] + 128;
					ram_read_targets_[18] = ram_write_targets_[16];
					ram_read_targets_[19] = ram_write_targets_[17];
				}
			break;
			case StaticAnalyser::Atari2600PagingModel::ActivisionStack:
				if(operation == CPU6502::BusOperation::ReadOpcode) {
					// if the last operation was either a JSR or an RTS, pick a new page
					// based on the address now being accesses
					if(last_opcode_ == 0x20 || last_opcode_ == 0x60) {
						if(address & 0x2000) {
							rom_pages_[0] = rom_;
						} else {
							rom_pages_[0] = &rom_[4096];
						}
						rom_pages_[1] = rom_pages_[0] + 1024;
						rom_pages_[2] = rom_pages_[0] + 2048;
						rom_pages_[3] = rom_pages_[0] + 3072;
					}
				}
			break;
		}

#undef AtariPager

		// check for a ROM read
		if(address&0x1000) {
			int ram_page = (masked_address & 0xfff) >> 7;
			ram_write_targets_[ram_page][masked_address & 0x7f] = *value;

			if(isReadOperation(operation)) {
				if(ram_read_targets_[ram_page]) {
					returnValue &= ram_read_targets_[ram_page][masked_address & 0x7f];
				} else {
					returnValue &= rom_pages_[(address >> 10)&3][address&1023];
				}
			}
		}

		// check for a RIOT RAM access
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
						returnValue &= 0;
					break;

					case 0x0c:
					case 0x0d:
						returnValue &= tia_input_value_[decodedAddress - 0x0c];
					break;
				}
			} else {
				const uint16_t decodedAddress = address & 0x3f;
				switch(decodedAddress) {
					case 0x00:	update_video(); tia_->set_sync(*value & 0x02);		break;
					case 0x01:	update_video();	tia_->set_blank(*value & 0x02);		break;

					case 0x02:	set_ready_line(true);								break;
					case 0x03:	update_video();	tia_->reset_horizontal_counter();	break;
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
					case 0x13:	update_video(); tia_->set_missile_position(decodedAddress - 0x12);					break;
					case 0x14:	update_video();	tia_->set_ball_position();											break;
					case 0x1b:
					case 0x1c:	update_video(); tia_->set_player_graphic(decodedAddress - 0x1b, *value);			break;
					case 0x1d:
					case 0x1e:	update_video(); tia_->set_missile_enable(decodedAddress - 0x1d, (*value)&2);		break;
					case 0x1f:	update_video(); tia_->set_ball_enable((*value)&2);									break;
					case 0x20:
					case 0x21:	update_video(); tia_->set_player_motion(decodedAddress - 0x20, *value);				break;
					case 0x22:
					case 0x23:	update_video(); tia_->set_missile_motion(decodedAddress - 0x22, *value);			break;
					case 0x24:	update_video(); tia_->set_ball_motion(*value);										break;
					case 0x25:
					case 0x26:	tia_->set_player_delay(decodedAddress - 0x25, (*value)&1);							break;
					case 0x27:	tia_->set_ball_delay((*value)&1);													break;
					case 0x28:
					case 0x29:	update_video(); tia_->set_missile_position_to_player(decodedAddress - 0x28, (*value)&2);		break;
					case 0x2a:	update_video(); tia_->move();														break;
					case 0x2b:	update_video(); tia_->clear_motion();												break;
					case 0x2c:	update_video(); tia_->clear_collision_flags();										break;

					case 0x15:
					case 0x16:	update_audio(); speaker_->set_control(decodedAddress - 0x15, *value);				break;
					case 0x17:
					case 0x18:	update_audio(); speaker_->set_divider(decodedAddress - 0x17, *value);				break;
					case 0x19:
					case 0x1a:	update_audio(); speaker_->set_volume(decodedAddress - 0x19, *value);				break;

					case 0x3f:
						if(paging_model_ == StaticAnalyser::Atari2600PagingModel::Tigervision && (masked_address == 0x3f)) {
							int selected_page = (*value) % (rom_size_ / 2048);
							rom_pages_[0] = &rom_[selected_page * 2048];
							rom_pages_[1] = rom_pages_[0] + 1024;
						}
					break;
				}
			}
		}

		// check for a PIA access
		if((address&0x1280) == 0x280) {
			update_6532();
			if(isReadOperation(operation)) {
				returnValue &= mos6532_.get_register(address);
			} else {
				mos6532_.set_register(address, *value);
			}
		}

		if(isReadOperation(operation)) {
			if(operation == CPU6502::BusOperation::ReadOpcode) last_opcode_ = returnValue;
			*value = returnValue;
		}
	}

	if(!tia_->get_cycles_until_horizontal_blank(cycles_since_video_update_)) set_ready_line(false);

	return cycles_run_for / 3;
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state) {
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

void Machine::set_switch_is_enabled(Atari2600Switch input, bool state) {
	switch(input) {
		case Atari2600SwitchReset:					mos6532_.update_port_input(1, 0x01, state);	break;
		case Atari2600SwitchSelect:					mos6532_.update_port_input(1, 0x02, state);	break;
		case Atari2600SwitchColour:					mos6532_.update_port_input(1, 0x08, state);	break;
		case Atari2600SwitchLeftPlayerDifficulty:	mos6532_.update_port_input(1, 0x40, state);	break;
		case Atari2600SwitchRightPlayerDifficulty:	mos6532_.update_port_input(1, 0x80, state);	break;
	}
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	if(!target.cartridges.front()->get_segments().size()) return;
	Storage::Cartridge::Cartridge::Segment segment = target.cartridges.front()->get_segments().front();
	size_t length = segment.data.size();

	rom_size_ = length;
	delete[] rom_;
	rom_ = new uint8_t[rom_size_];

	size_t offset = 0;
	const size_t copy_step = std::min(rom_size_, length);
	while(offset < rom_size_) {
		size_t copy_length = std::min(copy_step, rom_size_ - offset);
		memcpy(&rom_[offset], &segment.data[0], copy_length);
		offset += copy_length;
	}

	// On a real paged cartridge, any page may initially be visible. Various homebrew authors appear to have
	// decided the last page will always be initially visible. So do that.
	size_t rom_mask = rom_size_ - 1;
	uint8_t *rom_base = rom_;
	if(rom_size_ > 4096) rom_base = &rom_[rom_size_ - 4096];
	rom_pages_[0] = rom_base;
	rom_pages_[1] = &rom_base[1024 & rom_mask];
	rom_pages_[2] = &rom_base[2048 & rom_mask];
	rom_pages_[3] = &rom_base[3072 & rom_mask];

	// By default, throw all stores away, and don't ever read from RAM
	for(int c = 0; c < sizeof(ram_write_targets_) / sizeof(*ram_write_targets_); c++) {
		ram_write_targets_[c] = throwaway_ram_;
		ram_read_targets_[c] = nullptr;
	}

	switch(target.atari.paging_model) {
		default:
			if(target.atari.uses_superchip) {
				// allocate 128 bytes of RAM; allow writing from 0x1000, reading from 0x1080
				ram_.resize(128);
				ram_write_targets_[0] = ram_.data();
				ram_read_targets_[1] = ram_write_targets_[0];
			}
		break;
		case StaticAnalyser::Atari2600PagingModel::CBSRamPlus:
			// allocate 256 bytes of RAM; allow writing from 0x1000, reading from 0x1100
			ram_.resize(256);
			ram_write_targets_[0] = ram_.data();
			ram_write_targets_[1] = ram_write_targets_[0] + 128;
			ram_read_targets_[2] = ram_write_targets_[0];
			ram_read_targets_[3] = ram_write_targets_[1];
		break;
		case StaticAnalyser::Atari2600PagingModel::CommaVid:
			// allocate 1kb of RAM; allow reading from 0x1000, writing from 0x1400
			ram_.resize(1024);
			for(int c = 0; c < 8; c++) {
				ram_read_targets_[c] = ram_.data() + 128 * c;
				ram_write_targets_[c + 8] = ram_.data() + 128 * c;
			}
		break;
		case StaticAnalyser::Atari2600PagingModel::MegaBoy:
			mega_boy_page_ = 15;
		break;
		case StaticAnalyser::Atari2600PagingModel::MNetwork:
			ram_.resize(2048);
			// Put 256 bytes of RAM for writing at 0x1800 and reading at 0x1900
			ram_write_targets_[16] = ram_.data();
			ram_write_targets_[17] = ram_write_targets_[16] + 128;
			ram_read_targets_[18] = ram_write_targets_[16];
			ram_read_targets_[19] = ram_write_targets_[17];

			rom_pages_[0] = rom_;
			rom_pages_[1] = rom_pages_[0] + 1024;
			rom_pages_[2] = rom_pages_[0] + 2048;
			rom_pages_[3] = rom_pages_[0] + 3072;
		break;
	}

	paging_model_ = target.atari.paging_model;
}

#pragma mark - Audio and Video

void Machine::update_audio() {
	unsigned int audio_cycles = cycles_since_speaker_update_ / 114;

	speaker_->run_for_cycles(audio_cycles);
	cycles_since_speaker_update_ %= 114;
}

void Machine::update_video() {
	tia_->run_for_cycles((int)cycles_since_video_update_);
	cycles_since_video_update_ = 0;
}

void Machine::update_6532() {
	mos6532_.run_for_cycles(cycles_since_6532_update_);
	cycles_since_6532_update_ = 0;
}

void Machine::synchronise() {
	update_audio();
	update_video();
	speaker_->flush();
}

#pragma mark - CRT delegate

void Machine::crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, unsigned int number_of_frames, unsigned int number_of_unexpected_vertical_syncs) {
	const size_t number_of_frame_records = sizeof(frame_records_) / sizeof(frame_records_[0]);
	frame_records_[frame_record_pointer_ % number_of_frame_records].number_of_frames = number_of_frames;
	frame_records_[frame_record_pointer_ % number_of_frame_records].number_of_unexpected_vertical_syncs = number_of_unexpected_vertical_syncs;
	frame_record_pointer_ ++;

	if(frame_record_pointer_ >= 6) {
		unsigned int total_number_of_frames = 0;
		unsigned int total_number_of_unexpected_vertical_syncs = 0;
		for(size_t c = 0; c < number_of_frame_records; c++) {
			total_number_of_frames += frame_records_[c].number_of_frames;
			total_number_of_unexpected_vertical_syncs += frame_records_[c].number_of_unexpected_vertical_syncs;
		}

		if(total_number_of_unexpected_vertical_syncs >= total_number_of_frames >> 1) {
			for(size_t c = 0; c < number_of_frame_records; c++) {
				frame_records_[c].number_of_frames = 0;
				frame_records_[c].number_of_unexpected_vertical_syncs = 0;
			}
			is_ntsc_ ^= true;

			double clock_rate;
			if(is_ntsc_) {
				clock_rate = NTSC_clock_rate;
				tia_->set_output_mode(TIA::OutputMode::NTSC);
			} else {
				clock_rate = PAL_clock_rate;
				tia_->set_output_mode(TIA::OutputMode::PAL);
			}

			speaker_->set_input_rate((float)(clock_rate / 38.0));
			speaker_->set_high_frequency_cut_off((float)(clock_rate / (38.0 * 2.0)));
			set_clock_rate(clock_rate);
		}
	}
}
