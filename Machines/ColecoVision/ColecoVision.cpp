//
//  ColecoVision.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ColecoVision.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"
#include "../../Components/SN76489/SN76489.hpp"

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Coleco {
namespace Vision {

class Joystick: public Inputs::Joystick {
	public:
		std::vector<DigitalInput> get_inputs() override {
			return {
				DigitalInput(DigitalInput::Up),
				DigitalInput(DigitalInput::Down),
				DigitalInput(DigitalInput::Left),
				DigitalInput(DigitalInput::Right),

				DigitalInput(DigitalInput::Fire, 0),
				DigitalInput(DigitalInput::Fire, 1),

				DigitalInput('0'),	DigitalInput('1'),	DigitalInput('2'),
				DigitalInput('3'),	DigitalInput('4'),	DigitalInput('5'),
				DigitalInput('6'),	DigitalInput('7'),	DigitalInput('8'),
				DigitalInput('9'),	DigitalInput('*'),	DigitalInput('#'),
			};
		}

		void set_digital_input(const DigitalInput &digital_input, bool is_active) override {
			switch(digital_input.type) {
				default: return;

				case DigitalInput::Key:
					if(!is_active) keypad_ |= 0xf;
					else {
						uint8_t mask = 0xf;
						switch(digital_input.info.key.symbol) {
							case '8':	mask = 0x1;		break;
							case '4':	mask = 0x2;		break;
							case '5':	mask = 0x3;		break;
							case '7':	mask = 0x5;		break;
							case '#':	mask = 0x6;		break;
							case '2':	mask = 0x7;		break;
							case '*':	mask = 0x9;		break;
							case '0':	mask = 0xa;		break;
							case '9':	mask = 0xb;		break;
							case '3':	mask = 0xc;		break;
							case '1':	mask = 0xd;		break;
							case '6':	mask = 0xe;		break;
							default: break;
						}
						keypad_ = (keypad_ & 0xf0) | mask;
					}
				break;

				case DigitalInput::Up: 		if(is_active) direction_ &= ~0x01; else direction_ |= 0x01;	break;
				case DigitalInput::Right:	if(is_active) direction_ &= ~0x02; else direction_ |= 0x02;	break;
				case DigitalInput::Down:	if(is_active) direction_ &= ~0x04; else direction_ |= 0x04;	break;
				case DigitalInput::Left:	if(is_active) direction_ &= ~0x08; else direction_ |= 0x08;	break;
				case DigitalInput::Fire:
					switch(digital_input.info.control.index) {
						default: break;
						case 0:	if(is_active) direction_ &= ~0x40; else direction_ |= 0x40;	break;
						case 1:	if(is_active) keypad_ &= ~0x40; else keypad_ |= 0x40;		break;
					}
				break;
			}
		}

		uint8_t get_direction_input() {
			return direction_;
		}

		uint8_t get_keypad_input() {
			return keypad_;
		}

	private:
		uint8_t direction_ = 0xff;
		uint8_t keypad_ = 0xff;
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public JoystickMachine::Machine {

	public:
		ConcreteMachine() :
			z80_(*this),
			sn76489_(audio_queue_),
			speaker_(sn76489_) {
			speaker_.set_input_rate(3579545.0f / 2.0f);	// TODO: try to find out whether this is correct.
			set_clock_rate(3579545);
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);
		}

		std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS9918(TI::TMS9918::TMS9918A));
			get_crt()->set_output_device(Outputs::CRT::OutputDevice::Television);
		}

		void close_output() override {
			vdp_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return vdp_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		void configure_as_target(const Analyser::Static::Target &target) override {
			// Insert the media.
			insert_media(target.media);
		}

		bool insert_media(const Analyser::Static::Media &media) override {
			if(!media.cartridges.empty()) {
				const auto &segment = media.cartridges.front()->get_segments().front();
				cartridge_ = segment.data;
			}

			return true;
		}

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"ColecoVision",
				{
					"coleco.rom"
				});

			if(!roms[0]) return false;

			bios_ = *roms[0];
			bios_.resize(8192);

			return true;
		}

		// MARK: Z80::BusHandler
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			time_since_vdp_update_ += cycle.length;
			time_since_sn76489_update_ += cycle.length;

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_non_maskable_interrupt_line(true, time_until_interrupt_);
					update_video();
					time_until_interrupt_ = vdp_->get_time_until_interrupt();
				}
			}

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					if(address < 0x2000) {
						*cycle.value = bios_[address];
					} else if(address >= 0x6000 && address < 0x8000) {
						*cycle.value = ram_[address & 1023];
					} else if(address >= 0x8000) {
						*cycle.value = cartridge_[(address - 0x8000) % cartridge_.size()];	// This probably isn't how 24kb ROMs work?
					} else {
						*cycle.value = 0xff;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(address >= 0x6000 && address < 0x8000) {
						ram_[address & 1023] = *cycle.value;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch((address >> 5) & 7) {
						case 5:
							update_video();
							*cycle.value = vdp_->get_register(address);
							z80_.set_non_maskable_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						case 7: {
							const std::size_t joystick_id = (address&2) >> 1;
							Joystick *joystick = static_cast<Joystick *>(joysticks_[joystick_id].get());
							if(joysticks_in_keypad_mode_) {
								*cycle.value = joystick->get_keypad_input();
							} else {
								*cycle.value = joystick->get_direction_input();
							}
						} break;

						default:
							*cycle.value = 0xff;
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output: {
					const int eighth = (address >> 5) & 7;
					switch(eighth) {
						case 4: case 6:
							joysticks_in_keypad_mode_ = eighth == 4;
						break;

						case 5:
							update_video();
							vdp_->set_register(address, *cycle.value);
							z80_.set_non_maskable_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						case 7:
							update_audio();
							sn76489_.set_register(*cycle.value);
						break;

						default: break;
					}
				} break;

				default: break;
			}

			return HalfCycles(0);
		}

		void flush() {
			vdp_->run_for(time_since_vdp_update_.flush());
			update_audio();
			audio_queue_.perform();
		}

	private:
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(2)));
		}
		void update_video() {
			vdp_->run_for(time_since_vdp_update_.flush());
		}

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TI::SN76489 sn76489_;
		Outputs::Speaker::LowpassSpeaker<TI::SN76489> speaker_;

		std::vector<uint8_t> bios_;
		std::vector<uint8_t> cartridge_;
		uint8_t ram_[1024];

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		bool joysticks_in_keypad_mode_ = false;

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_interrupt_;
};

}
}

using namespace Coleco::Vision;

Machine *Machine::ColecoVision() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
