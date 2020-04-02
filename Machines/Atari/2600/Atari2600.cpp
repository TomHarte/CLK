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

#include "../../MachineTypes.hpp"

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

		void did_set_input(const Input &digital_input, bool is_active) final {
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
	public MachineTypes::TimedMachine,
	public MachineTypes::AudioProducer,
	public MachineTypes::ScanProducer,
	public MachineTypes::JoystickMachine {
	public:
		ConcreteMachine(const Target &target) : frequency_mismatch_warner_(*this) {
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

			set_is_ntsc(is_ntsc_);
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return joysticks_;
		}

		void set_switch_is_enabled(Atari2600Switch input, bool state) final {
			switch(input) {
				case Atari2600SwitchReset:					bus_->mos6532_.update_port_input(1, 0x01, state);	break;
				case Atari2600SwitchSelect:					bus_->mos6532_.update_port_input(1, 0x02, state);	break;
				case Atari2600SwitchColour:					bus_->mos6532_.update_port_input(1, 0x08, state);	break;
				case Atari2600SwitchLeftPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x40, state);	break;
				case Atari2600SwitchRightPlayerDifficulty:	bus_->mos6532_.update_port_input(1, 0x80, state);	break;
			}
		}

		bool get_switch_is_enabled(Atari2600Switch input) final {
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

		void set_reset_switch(bool state) final {
			bus_->set_reset_line(state);
		}

		// to satisfy CRTMachine::Machine
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			bus_->speaker_.set_input_rate(static_cast<float>(get_clock_rate() / static_cast<double>(CPUTicksPerAudioTick)));
			bus_->tia_.set_crt_delegate(&frequency_mismatch_warner_);
			bus_->tia_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return bus_->tia_.get_scaled_scan_status() / 3.0f;
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &bus_->speaker_;
		}

		void run_for(const Cycles cycles) final {
			bus_->run_for(cycles);
			bus_->apply_confidence(confidence_counter_);
		}

		void flush() {
			bus_->flush();
		}

		void register_crt_frequency_mismatch() {
			is_ntsc_ ^= true;
			set_is_ntsc(is_ntsc_);
		}

		float get_confidence() final {
			return confidence_counter_.get_confidence();
		}

	private:
		// The bus.
		std::unique_ptr<Bus> bus_;

		// Output frame rate tracker.
		Outputs::CRT::CRTFrequencyMismatchWarner<ConcreteMachine> frequency_mismatch_warner_;
		bool is_ntsc_ = true;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		// a confidence counter
		Analyser::Dynamic::ConfidenceCounter confidence_counter_;

		void set_is_ntsc(bool is_ntsc) {
			bus_->tia_.set_output_mode(is_ntsc ? TIA::OutputMode::NTSC : TIA::OutputMode::PAL);
			const double clock_rate = is_ntsc ? NTSC_clock_rate : PAL_clock_rate;
			bus_->speaker_.set_input_rate(float(clock_rate) / float(CPUTicksPerAudioTick));
			bus_->speaker_.set_high_frequency_cutoff(float(clock_rate) / float(CPUTicksPerAudioTick * 2));
			set_clock_rate(clock_rate);
		}
};

}

using namespace Atari2600;

Machine *Machine::Atari2600(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const Target *const atari_target = dynamic_cast<const Target *>(target);
	return new Atari2600::ConcreteMachine(*atari_target);
}

Machine::~Machine() {}
