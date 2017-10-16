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

#include "Cartridges/Atari8k.hpp"
#include "Cartridges/Atari16k.hpp"
#include "Cartridges/Atari32k.hpp"
#include "Cartridges/ActivisionStack.hpp"
#include "Cartridges/CBSRAMPlus.hpp"
#include "Cartridges/CommaVid.hpp"
#include "Cartridges/MegaBoy.hpp"
#include "Cartridges/MNetwork.hpp"
#include "Cartridges/ParkerBros.hpp"
#include "Cartridges/Pitfall2.hpp"
#include "Cartridges/Tigervision.hpp"
#include "Cartridges/Unpaged.hpp"

namespace {
	static const double NTSC_clock_rate = 1194720;
	static const double PAL_clock_rate = 1182298;
}

namespace Atari2600 {

class Joystick: public Inputs::Joystick {
	public:
		Joystick(Bus *bus, size_t shift, size_t fire_tia_input) :
			bus_(bus), shift_(shift), fire_tia_input_(fire_tia_input) {}

		void set_digital_input(DigitalInput digital_input, bool is_active) {
			switch(digital_input) {
				case DigitalInput::Up:		bus_->mos6532_.update_port_input(0, 0x10 >> shift_, is_active);		break;
				case DigitalInput::Down:	bus_->mos6532_.update_port_input(0, 0x20 >> shift_, is_active);		break;
				case DigitalInput::Left:	bus_->mos6532_.update_port_input(0, 0x40 >> shift_, is_active);		break;
				case DigitalInput::Right:	bus_->mos6532_.update_port_input(0, 0x80 >> shift_, is_active);		break;

				// TODO: latching
				case DigitalInput::Fire:
					if(is_active)
						bus_->tia_input_value_[fire_tia_input_] &= ~0x80;
					else
						bus_->tia_input_value_[fire_tia_input_] |= 0x80;
				break;
			}
		}

	private:
		size_t shift_, fire_tia_input_;
		Bus *bus_;
};

class ConcreteMachine:
	public Machine,
	public Outputs::CRT::Delegate {
	public:
		ConcreteMachine() :
			frame_record_pointer_(0),
			is_ntsc_(true) {
			set_clock_rate(NTSC_clock_rate);
		}

		~ConcreteMachine() {
			close_output();
		}

		void configure_as_target(const StaticAnalyser::Target &target) override {
			const std::vector<uint8_t> &rom = target.media.cartridges.front()->get_segments().front().data;
			switch(target.atari.paging_model) {
				case StaticAnalyser::Atari2600PagingModel::ActivisionStack:	bus_.reset(new Cartridge::Cartridge<Cartridge::ActivisionStack>(rom));	break;
				case StaticAnalyser::Atari2600PagingModel::CBSRamPlus:		bus_.reset(new Cartridge::Cartridge<Cartridge::CBSRAMPlus>(rom));		break;
				case StaticAnalyser::Atari2600PagingModel::CommaVid:		bus_.reset(new Cartridge::Cartridge<Cartridge::CommaVid>(rom));			break;
				case StaticAnalyser::Atari2600PagingModel::MegaBoy:			bus_.reset(new Cartridge::Cartridge<Cartridge::MegaBoy>(rom));			break;
				case StaticAnalyser::Atari2600PagingModel::MNetwork:		bus_.reset(new Cartridge::Cartridge<Cartridge::MNetwork>(rom));			break;
				case StaticAnalyser::Atari2600PagingModel::None:			bus_.reset(new Cartridge::Cartridge<Cartridge::Unpaged>(rom));			break;
				case StaticAnalyser::Atari2600PagingModel::ParkerBros:		bus_.reset(new Cartridge::Cartridge<Cartridge::ParkerBros>(rom));		break;
				case StaticAnalyser::Atari2600PagingModel::Pitfall2:		bus_.reset(new Cartridge::Cartridge<Cartridge::Pitfall2>(rom));			break;
				case StaticAnalyser::Atari2600PagingModel::Tigervision:		bus_.reset(new Cartridge::Cartridge<Cartridge::Tigervision>(rom));		break;

				case StaticAnalyser::Atari2600PagingModel::Atari8k:
					if(target.atari.uses_superchip) {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari8kSuperChip>(rom));
					} else {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari8k>(rom));
					}
				break;
				case StaticAnalyser::Atari2600PagingModel::Atari16k:
					if(target.atari.uses_superchip) {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari16kSuperChip>(rom));
					} else {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari16k>(rom));
					}
				break;
				case StaticAnalyser::Atari2600PagingModel::Atari32k:
					if(target.atari.uses_superchip) {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari32kSuperChip>(rom));
					} else {
						bus_.reset(new Cartridge::Cartridge<Cartridge::Atari32k>(rom));
					}
				break;
			}

			joysticks_.push_back(new Joystick(bus_.get(), 0, 0));
			joysticks_.push_back(new Joystick(bus_.get(), 4, 1));
		}

		bool insert_media(const StaticAnalyser::Media &media) override {
			return false;
		}

		std::vector<Inputs::Joystick *> &get_joysticks() override {
			return joysticks_;
		}

		void set_switch_is_enabled(Atari2600Switch input, bool state) override {
			switch(input) {
				case Atari2600SwitchReset:					bus_->mos6532_.update_port_input(1, 0x01, state);	break;
				case Atari2600SwitchSelect:					bus_->mos6532_.update_port_input(1, 0x02, state);	break;
				case Atari2600SwitchColour:					bus_->mos6532_.update_port_input(1, 0x08, state);	break;
				case Atari2600SwitchLeftPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x40, state);	break;
				case Atari2600SwitchRightPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x80, state);	break;
			}
		}

		void set_reset_switch(bool state) override {
			bus_->set_reset_line(state);
		}

		// to satisfy CRTMachine::Machine
		void setup_output(float aspect_ratio) override {
			bus_->tia_.reset(new TIA);
			bus_->speaker_.reset(new Speaker);
			bus_->speaker_->set_input_rate((float)(get_clock_rate() / (double)CPUTicksPerAudioTick));
			bus_->tia_->get_crt()->set_delegate(this);
		}

		void close_output() override {
			bus_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override {
			return bus_->tia_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override {
			return bus_->speaker_;
		}

		void run_for(const Cycles cycles) override {
			bus_->run_for(cycles);
		}

		// to satisfy Outputs::CRT::Delegate
		void crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, unsigned int number_of_frames, unsigned int number_of_unexpected_vertical_syncs) override {
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

					bus_->speaker_->set_input_rate((float)(clock_rate / (double)CPUTicksPerAudioTick));
					bus_->speaker_->set_high_frequency_cut_off((float)(clock_rate / ((double)CPUTicksPerAudioTick * 2.0)));
					set_clock_rate(clock_rate);
				}
			}
		}

	private:
		// the bus
		std::unique_ptr<Bus> bus_;

		// output frame rate tracker
		struct FrameRecord {
			unsigned int number_of_frames;
			unsigned int number_of_unexpected_vertical_syncs;

			FrameRecord() : number_of_frames(0), number_of_unexpected_vertical_syncs(0) {}
		} frame_records_[4];
		unsigned int frame_record_pointer_;
		bool is_ntsc_;
		std::vector<Inputs::Joystick *> joysticks_;
};

}

using namespace Atari2600;

Machine *Machine::Atari2600() {
	return new Atari2600::ConcreteMachine;
}

Machine::~Machine() {}
