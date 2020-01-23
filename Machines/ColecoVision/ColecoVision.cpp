//
//  ColecoVision.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "ColecoVision.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"
#include "../../Components/AY38910/AY38910.hpp"	// For the Super Game Module.
#include "../../Components/SN76489/SN76489.hpp"

#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"

#include "../../Configurable/StandardOptions.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../ClockReceiver/JustInTime.hpp"

#include "../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Analyser/Dynamic/ConfidenceCounter.hpp"

namespace {
constexpr int sn76489_divider = 2;
}

namespace Coleco {
namespace Vision {

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(Configurable::DisplaySVideo | Configurable::DisplayCompositeColour)
	);
}

class Joystick: public Inputs::ConcreteJoystick {
	public:
		Joystick() :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),

				Input(Input::Fire, 0),
				Input(Input::Fire, 1),

				Input('0'),	Input('1'),	Input('2'),
				Input('3'),	Input('4'),	Input('5'),
				Input('6'),	Input('7'),	Input('8'),
				Input('9'),	Input('*'),	Input('#'),
			}) {}

		void did_set_input(const Input &digital_input, bool is_active) override {
			switch(digital_input.type) {
				default: return;

				case Input::Key:
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

				case Input::Up:		if(is_active) direction_ &= ~0x01; else direction_ |= 0x01;	break;
				case Input::Right:	if(is_active) direction_ &= ~0x02; else direction_ |= 0x02;	break;
				case Input::Down:	if(is_active) direction_ &= ~0x04; else direction_ |= 0x04;	break;
				case Input::Left:	if(is_active) direction_ &= ~0x08; else direction_ |= 0x08;	break;
				case Input::Fire:
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
		uint8_t keypad_ = 0x7f;
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public Configurable::Device,
	public JoystickMachine::Machine {

	public:
		ConcreteMachine(const Analyser::Static::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this),
			vdp_(TI::TMS::TMS9918A),
			sn76489_(TI::SN76489::Personality::SN76489, audio_queue_, sn76489_divider),
			ay_(GI::AY38910::Personality::AY38910, audio_queue_),
			mixer_(sn76489_, ay_),
			speaker_(mixer_) {
			speaker_.set_input_rate(3579545.0f / static_cast<float>(sn76489_divider));
			set_clock_rate(3579545);
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);

			const auto roms = rom_fetcher(
				{ {"ColecoVision", "the ColecoVision BIOS", "coleco.rom", 8*1024, 0x3aa93ef3} });

			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}

			bios_ = *roms[0];
			bios_.resize(8192);

			if(!target.media.cartridges.empty()) {
				const auto &segment = target.media.cartridges.front()->get_segments().front();
				cartridge_ = segment.data;
				if(cartridge_.size() >= 32768)
					cartridge_address_limit_ = 0xffff;
				else
					cartridge_address_limit_ = static_cast<uint16_t>(0x8000 + cartridge_.size() - 1);

				if(cartridge_.size() > 32768) {
					// Ensure the cartrige is a multiple of 16kb in size, as that won't
					// be checked when paging.
					const size_t extension = (16384 - (cartridge_.size() & 16383)) % 16384;
					cartridge_.resize(cartridge_.size() + extension);

					cartridge_pages_[0] = &cartridge_[cartridge_.size() - 16384];
					cartridge_pages_[1] = cartridge_.data();
					is_megacart_ = true;
				} else {
					// Ensure at least 32kb is allocated to the cartrige so that
					// reads are never out of bounds.
					cartridge_.resize(32768);

					cartridge_pages_[0] = cartridge_.data();
					cartridge_pages_[1] = cartridge_.data() + 16384;
					is_megacart_ = false;
				}
			}

			// ColecoVisions have composite output only.
			vdp_->set_display_type(Outputs::Display::DisplayType::CompositeColour);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			vdp_->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return vdp_->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) override {
			vdp_->set_display_type(display_type);
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		// MARK: Z80::BusHandler
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// The SN76489 will use its ready line to trigger the Z80's wait, which will add
			// thirty-one (!) cycles when accessed. M1 cycles are extended by a single cycle.
			// This code works out the delay up front in order to simplify execution flow, though
			// technically this is a little duplicative.
			HalfCycles penalty(0);
			if(cycle.operation == CPU::Z80::PartialMachineCycle::Output && ((*cycle.address >> 5) & 7) == 7) {
				penalty = HalfCycles(62);
			} else if(cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) {
				penalty = HalfCycles(2);
			}
			const HalfCycles length = cycle.length + penalty;

			vdp_ += length;
			time_since_sn76489_update_ += length;

			// Act only if necessary.
			if(cycle.is_terminal()) {
				uint16_t address = cycle.address ? *cycle.address : 0x0000;
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
						if(!address) pc_zero_accesses_++;
					case CPU::Z80::PartialMachineCycle::Read:
						if(address < 0x2000) {
							if(super_game_module_.replace_bios) {
								*cycle.value = super_game_module_.ram[address];
							} else {
								*cycle.value = bios_[address];
							}
						} else if(super_game_module_.replace_ram && address < 0x8000) {
							*cycle.value = super_game_module_.ram[address];
						} else if(address >= 0x6000 && address < 0x8000) {
							*cycle.value = ram_[address & 1023];
						} else if(address >= 0x8000 && address <= cartridge_address_limit_) {
							if(is_megacart_ && address >= 0xffc0) {
								page_megacart(address);
							}
							*cycle.value = cartridge_pages_[(address >> 14)&1][address&0x3fff];
						} else {
							*cycle.value = 0xff;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Write:
						if(super_game_module_.replace_bios && address < 0x2000) {
							super_game_module_.ram[address] = *cycle.value;
						} else if(super_game_module_.replace_ram && address >= 0x2000 && address < 0x8000) {
							super_game_module_.ram[address] = *cycle.value;
						} else if(address >= 0x6000 && address < 0x8000) {
							ram_[address & 1023] = *cycle.value;
						} else if(is_megacart_ && address >= 0xffc0) {
							page_megacart(address);
						}
					break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch((address >> 5) & 7) {
							case 5:
								*cycle.value = vdp_->read(address);
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

								// Hitting exactly the recommended joypad input port is an indicator that
								// this really is a ColecoVision game. The BIOS won't do this when just waiting
								// to start a game (unlike accessing the VDP and SN).
								if((address&0xfc) == 0xfc) confidence_counter_.add_hit();
							} break;

							default:
								switch(address&0xff) {
									default: *cycle.value = 0xff; break;
									case 0x52:
										// Read AY data.
										update_audio();
										ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BC1));
										*cycle.value = ay_.get_data_output();
										ay_.set_control_lines(GI::AY38910::ControlLines(0));
									break;
								}
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
								vdp_->write(address, *cycle.value);
								z80_.set_non_maskable_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;

							case 7:
								update_audio();
								sn76489_.write(*cycle.value);
							break;

							default:
								// Catch Super Game Module accesses; it decodes more thoroughly.
								switch(address&0xff) {
									default: break;
									case 0x7f:
										super_game_module_.replace_bios = !((*cycle.value)&0x2);
									break;
									case 0x50:
										// Set AY address.
										update_audio();
										ay_.set_control_lines(GI::AY38910::BC1);
										ay_.set_data_input(*cycle.value);
										ay_.set_control_lines(GI::AY38910::ControlLines(0));
									break;
									case 0x51:
										// Set AY data.
										update_audio();
										ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BDIR));
										ay_.set_data_input(*cycle.value);
										ay_.set_control_lines(GI::AY38910::ControlLines(0));
									break;
									case 0x53:
										super_game_module_.replace_ram = !!((*cycle.value)&0x1);
									break;
								}
							break;
						}
					} break;

					default: break;
				}
			}

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_non_maskable_interrupt_line(true, time_until_interrupt_);
				}
			}

			return penalty;
		}

		void flush() {
			vdp_.flush();
			update_audio();
			audio_queue_.perform();
			audio_queue_.flush();
		}

		float get_confidence() override {
			if(pc_zero_accesses_ > 1) return 0.0f;
			return confidence_counter_.get_confidence();
		}

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return Coleco::Vision::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			Configurable::Display display;
			if(Configurable::get_display(selections_by_option, display)) {
				set_video_signal_configurable(display);
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_display_selection(selection_set, Configurable::Display::CompositeColour);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_display_selection(selection_set, Configurable::Display::SVideo);
			return selection_set;
		}

	private:
		inline void page_megacart(uint16_t address) {
			const std::size_t selected_start = (static_cast<std::size_t>(address&63) << 14) % cartridge_.size();
			cartridge_pages_[1] = &cartridge_[selected_start];
		}
		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(sn76489_divider)));
		}

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		JustInTimeActor<TI::TMS::TMS9918, 1, 1, HalfCycles> vdp_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TI::SN76489 sn76489_;
		GI::AY38910::AY38910 ay_;
		Outputs::Speaker::CompoundSource<TI::SN76489, GI::AY38910::AY38910> mixer_;
		Outputs::Speaker::LowpassSpeaker<Outputs::Speaker::CompoundSource<TI::SN76489, GI::AY38910::AY38910>> speaker_;

		std::vector<uint8_t> bios_;
		std::vector<uint8_t> cartridge_;
		uint8_t *cartridge_pages_[2];
		uint8_t ram_[1024];
		bool is_megacart_ = false;
		uint16_t cartridge_address_limit_ = 0;
		struct {
			bool replace_bios = false;
			bool replace_ram = false;
			uint8_t ram[32768];
		} super_game_module_;

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		bool joysticks_in_keypad_mode_ = false;

		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_interrupt_;

		Analyser::Dynamic::ConfidenceCounter confidence_counter_;
		int pc_zero_accesses_ = 0;
};

}
}

using namespace Coleco::Vision;

Machine *Machine::ColecoVision(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
