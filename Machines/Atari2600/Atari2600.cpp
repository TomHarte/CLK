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

#include "CartridgeAtari8k.hpp"
#include "CartridgeAtari16k.hpp"
#include "CartridgeAtari32k.hpp"
#include "CartridgeActivisionStack.hpp"
#include "CartridgeCBSRAMPlus.hpp"
#include "CartridgeCommaVid.hpp"
#include "CartridgeParkerBros.hpp"
#include "CartridgeTigervision.hpp"
#include "CartridgeUnpaged.hpp"

using namespace Atari2600;
namespace {
	static const double NTSC_clock_rate = 1194720;
	static const double PAL_clock_rate = 1182298;
}

Machine::Machine() :
	rom_(nullptr),
	rom_pages_{nullptr, nullptr, nullptr, nullptr},
	frame_record_pointer_(0),
	is_ntsc_(true) {
	set_clock_rate(NTSC_clock_rate);
}

void Machine::setup_output(float aspect_ratio) {
	bus_->tia_.reset(new TIA);
	bus_->speaker_.reset(new Speaker);
	bus_->speaker_->set_input_rate((float)(get_clock_rate() / 38.0));
	bus_->tia_->get_crt()->set_delegate(this);
}

void Machine::close_output() {
	bus_.reset();
}

Machine::~Machine() {
	close_output();
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state) {
	switch (input) {
		case Atari2600DigitalInputJoy1Up:		bus_->mos6532_.update_port_input(0, 0x10, state);	break;
		case Atari2600DigitalInputJoy1Down:		bus_->mos6532_.update_port_input(0, 0x20, state);	break;
		case Atari2600DigitalInputJoy1Left:		bus_->mos6532_.update_port_input(0, 0x40, state);	break;
		case Atari2600DigitalInputJoy1Right:	bus_->mos6532_.update_port_input(0, 0x80, state);	break;

		case Atari2600DigitalInputJoy2Up:		bus_->mos6532_.update_port_input(0, 0x01, state);	break;
		case Atari2600DigitalInputJoy2Down:		bus_->mos6532_.update_port_input(0, 0x02, state);	break;
		case Atari2600DigitalInputJoy2Left:		bus_->mos6532_.update_port_input(0, 0x04, state);	break;
		case Atari2600DigitalInputJoy2Right:	bus_->mos6532_.update_port_input(0, 0x08, state);	break;

		// TODO: latching
		case Atari2600DigitalInputJoy1Fire:		if(state) bus_->tia_input_value_[0] &= ~0x80; else bus_->tia_input_value_[0] |= 0x80; break;
		case Atari2600DigitalInputJoy2Fire:		if(state) bus_->tia_input_value_[1] &= ~0x80; else bus_->tia_input_value_[1] |= 0x80; break;

		default: break;
	}
}

void Machine::set_switch_is_enabled(Atari2600Switch input, bool state) {
	switch(input) {
		case Atari2600SwitchReset:					bus_->mos6532_.update_port_input(1, 0x01, state);	break;
		case Atari2600SwitchSelect:					bus_->mos6532_.update_port_input(1, 0x02, state);	break;
		case Atari2600SwitchColour:					bus_->mos6532_.update_port_input(1, 0x08, state);	break;
		case Atari2600SwitchLeftPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x40, state);	break;
		case Atari2600SwitchRightPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x80, state);	break;
	}
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	const std::vector<uint8_t> &rom = target.cartridges.front()->get_segments().front().data;
	switch(target.atari.paging_model) {
		case StaticAnalyser::Atari2600PagingModel::None:			bus_.reset(new CartridgeUnpaged(rom));			break;
		case StaticAnalyser::Atari2600PagingModel::CommaVid:		bus_.reset(new CartridgeCommaVid(rom));			break;
		case StaticAnalyser::Atari2600PagingModel::ActivisionStack:	bus_.reset(new CartridgeActivisionStack(rom));	break;
		case StaticAnalyser::Atari2600PagingModel::ParkerBros:		bus_.reset(new CartridgeParkerBros(rom));		break;
		case StaticAnalyser::Atari2600PagingModel::Tigervision:		bus_.reset(new CartridgeTigervision(rom));		break;
		case StaticAnalyser::Atari2600PagingModel::CBSRamPlus:		bus_.reset(new CartridgeCBSRAMPlus(rom));		break;
		case StaticAnalyser::Atari2600PagingModel::Atari8k:
			if(target.atari.uses_superchip) {
				bus_.reset(new CartridgeAtari8kSuperChip(rom));
			} else {
				bus_.reset(new CartridgeAtari8k(rom));
			}
		break;
		case StaticAnalyser::Atari2600PagingModel::Atari16k:
			if(target.atari.uses_superchip) {
				bus_.reset(new CartridgeAtari16kSuperChip(rom));
			} else {
				bus_.reset(new CartridgeAtari16k(rom));
			}
		break;
		case StaticAnalyser::Atari2600PagingModel::Atari32k:
			if(target.atari.uses_superchip) {
				bus_.reset(new CartridgeAtari32kSuperChip(rom));
			} else {
				bus_.reset(new CartridgeAtari32k(rom));
			}
		break;
	}

/*	switch(target.atari.paging_model) {
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
	}*/
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
				bus_->tia_->set_output_mode(TIA::OutputMode::NTSC);
			} else {
				clock_rate = PAL_clock_rate;
				bus_->tia_->set_output_mode(TIA::OutputMode::PAL);
			}

			bus_->speaker_->set_input_rate((float)(clock_rate / 38.0));
			bus_->speaker_->set_high_frequency_cut_off((float)(clock_rate / (38.0 * 2.0)));
			set_clock_rate(clock_rate);
		}
	}
}
