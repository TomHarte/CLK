//
//  Atari2600.cpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#include "Atari2600.hpp"

#include <algorithm>
#include <cstdio>

#include "../../CRTMachine.hpp"
#include "../../JoystickMachine.hpp"

#include "../../../Analyser/Static/Atari2600/Target.hpp"

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

class Joystick: public Inputs::ConcreteJoystick {
	public:
		Joystick(Bus *bus, std::size_t shift, std::size_t fire_tia_input) :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),
				Input(Input::Fire)
			}),
			bus_(bus), shift_(shift), fire_tia_input_(fire_tia_input) {}

		void did_set_input(const Input &digital_input, bool is_active) override {
			switch(digital_input.type) {
				case Input::Up:		bus_->mos6532_.update_port_input(0, 0x10 >> shift_, is_active);		break;
				case Input::Down:	bus_->mos6532_.update_port_input(0, 0x20 >> shift_, is_active);		break;
				case Input::Left:	bus_->mos6532_.update_port_input(0, 0x40 >> shift_, is_active);		break;
				case Input::Right:	bus_->mos6532_.update_port_input(0, 0x80 >> shift_, is_active);		break;

				// TODO: latching
				case Input::Fire:
					if(is_active)
						bus_->tia_input_value_[fire_tia_input_] &= ~0x80;
					else
						bus_->tia_input_value_[fire_tia_input_] |= 0x80;
				break;

				default: break;
			}
		}

	private:
		Bus *bus_;
		std::size_t shift_, fire_tia_input_;
};

using Target = Analyser::Static::Atari2600::Target;

class ConcreteMachine:
	public Machine,
	public CRTMachine::Machine,
	public JoystickMachine::Machine,
	public Outputs::CRT::Delegate {
	public:
		ConcreteMachine(const Target &target) {
			set_clock_rate(NTSC_clock_rate);

			const std::vector<uint8_t> &rom = target.media.cartridges.front()->get_segments().front().data;

			using PagingModel = Target::PagingModel;
			switch(target.paging_model) {
				case PagingModel::ActivisionStack:	bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::ActivisionStack>>(rom);	break;
				case PagingModel::CBSRamPlus:		bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::CBSRAMPlus>>(rom);		break;
				case PagingModel::CommaVid:			bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::CommaVid>>(rom);		break;
				case PagingModel::MegaBoy:			bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::MegaBoy>>(rom);			break;
				case PagingModel::MNetwork:			bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::MNetwork>>(rom);		break;
				case PagingModel::None:				bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Unpaged>>(rom);			break;
				case PagingModel::ParkerBros:		bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::ParkerBros>>(rom);		break;
				case PagingModel::Pitfall2:			bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Pitfall2>>(rom);		break;
				case PagingModel::Tigervision:		bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Tigervision>>(rom);		break;

				case PagingModel::Atari8k:
					if(target.uses_superchip) {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari8kSuperChip>>(rom);
					} else {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari8k>>(rom);
					}
				break;
				case PagingModel::Atari16k:
					if(target.uses_superchip) {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari16kSuperChip>>(rom);
					} else {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari16k>>(rom);
					}
				break;
				case PagingModel::Atari32k:
					if(target.uses_superchip) {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari32kSuperChip>>(rom);
					} else {
						bus_ = std::make_unique<Cartridge::Cartridge<Cartridge::Atari32k>>(rom);
					}
				break;
			}

			joysticks_.emplace_back(new Joystick(bus_.get(), 0, 0));
			joysticks_.emplace_back(new Joystick(bus_.get(), 4, 1));
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
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

		bool get_switch_is_enabled(Atari2600Switch input) override {
			uint8_t port_input = bus_->mos6532_.get_port_input(1);
			switch(input) {
				case Atari2600SwitchReset:					return !!(port_input & 0x01);
				case Atari2600SwitchSelect:					return !!(port_input & 0x02);
				case Atari2600SwitchColour:					return !!(port_input & 0x08);
				case Atari2600SwitchLeftPlayerDifficulty:	return !!(port_input & 0x40);
				case Atari2600SwitchRightPlayerDifficulty:	return !!(port_input & 0x80);
				default:									return false;
			}
		}

		void set_reset_switch(bool state) override {
			bus_->set_reset_line(state);
		}

		// to satisfy CRTMachine::Machine
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			bus_->speaker_.set_input_rate(static_cast<float>(get_clock_rate() / static_cast<double>(CPUTicksPerAudioTick)));
			bus_->tia_.set_crt_delegate(this);
			bus_->tia_.set_scan_target(scan_target);
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &bus_->speaker_;
		}

		void run_for(const Cycles cycles) override {
			bus_->run_for(cycles);
			bus_->apply_confidence(confidence_counter_);
		}

		// to satisfy Outputs::CRT::Delegate
		void crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, int number_of_frames, int number_of_unexpected_vertical_syncs) override {
			const std::size_t number_of_frame_records = sizeof(frame_records_) / sizeof(frame_records_[0]);
			frame_records_[frame_record_pointer_ % number_of_frame_records].number_of_frames = number_of_frames;
			frame_records_[frame_record_pointer_ % number_of_frame_records].number_of_unexpected_vertical_syncs = number_of_unexpected_vertical_syncs;
			frame_record_pointer_ ++;

			if(frame_record_pointer_ >= 6) {
				int total_number_of_frames = 0;
				int total_number_of_unexpected_vertical_syncs = 0;
				for(std::size_t c = 0; c < number_of_frame_records; c++) {
					total_number_of_frames += frame_records_[c].number_of_frames;
					total_number_of_unexpected_vertical_syncs += frame_records_[c].number_of_unexpected_vertical_syncs;
				}

				if(total_number_of_unexpected_vertical_syncs >= total_number_of_frames >> 1) {
					for(std::size_t c = 0; c < number_of_frame_records; c++) {
						frame_records_[c].number_of_frames = 0;
						frame_records_[c].number_of_unexpected_vertical_syncs = 0;
					}
					is_ntsc_ ^= true;

					double clock_rate;
					if(is_ntsc_) {
						clock_rate = NTSC_clock_rate;
						bus_->tia_.set_output_mode(TIA::OutputMode::NTSC);
					} else {
						clock_rate = PAL_clock_rate;
						bus_->tia_.set_output_mode(TIA::OutputMode::PAL);
					}

					bus_->speaker_.set_input_rate(static_cast<float>(clock_rate / static_cast<double>(CPUTicksPerAudioTick)));
					bus_->speaker_.set_high_frequency_cutoff(static_cast<float>(clock_rate / (static_cast<double>(CPUTicksPerAudioTick) * 2.0)));
					set_clock_rate(clock_rate);
				}
			}
		}

		float get_confidence() override {
			return confidence_counter_.get_confidence();
		}

	private:
		// the bus
		std::unique_ptr<Bus> bus_;

		// output frame rate tracker
		struct FrameRecord {
			int number_of_frames = 0;
			int number_of_unexpected_vertical_syncs = 0;
		} frame_records_[4];
		unsigned int frame_record_pointer_ = 0;
		bool is_ntsc_ = true;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		// a confidence counter
		Analyser::Dynamic::ConfidenceCounter confidence_counter_;
};

}

using namespace Atari2600;

Machine *Machine::Atari2600(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const Target *const atari_target = dynamic_cast<const Target *>(target);
	return new Atari2600::ConcreteMachine(*atari_target);
}

Machine::~Machine() {}
