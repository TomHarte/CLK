//
//  MasterSystem.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MasterSystem.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"
#include "../../Components/SN76489/SN76489.hpp"
#include "../../Components/OPx/OPLL.hpp"

#include "../MachineTypes.hpp"
#include "../../Configurable/Configurable.hpp"

#include "../../ClockReceiver/ForceInline.hpp"
#include "../../ClockReceiver/JustInTime.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Speaker/Implementation/CompoundSource.hpp"

#define LOG_PREFIX "[SMS] "
#include "../../Outputs/Log.hpp"

#include "../../Analyser/Static/Sega/Target.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace {
constexpr int audio_divider = 1;
}

namespace Sega {
namespace MasterSystem {

class Joystick: public Inputs::ConcreteJoystick {
	public:
		Joystick() :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),

				Input(Input::Fire, 0),
				Input(Input::Fire, 1)
			}) {}

		void did_set_input(const Input &digital_input, bool is_active) final {
			switch(digital_input.type) {
				default: return;

				case Input::Up:		if(is_active) state_ &= ~0x01; else state_ |= 0x01;	break;
				case Input::Down:	if(is_active) state_ &= ~0x02; else state_ |= 0x02;	break;
				case Input::Left:	if(is_active) state_ &= ~0x04; else state_ |= 0x04;	break;
				case Input::Right:	if(is_active) state_ &= ~0x08; else state_ |= 0x08;	break;
				case Input::Fire:
					switch(digital_input.info.control.index) {
						default: break;
						case 0:	if(is_active) state_ &= ~0x10; else state_ |= 0x10;		break;
						case 1:	if(is_active) state_ &= ~0x20; else state_ |= 0x20;		break;
					}
				break;
			}
		}

		uint8_t get_state() {
			return state_;
		}

	private:
		uint8_t state_ = 0xff;
};

template <Analyser::Static::Sega::Target::Model model> class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::KeyboardMachine,
	public MachineTypes::JoystickMachine,
	public Configurable::Device,
	public Inputs::Keyboard::Delegate {

	public:
		ConcreteMachine(const Analyser::Static::Sega::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			region_(target.region),
			paging_scheme_(target.paging_scheme),
			z80_(*this),
			sn76489_(
				(target.model == Target::Model::SG1000) ? TI::SN76489::Personality::SN76489 : TI::SN76489::Personality::SMS,
				audio_queue_,
				audio_divider),
			opll_(audio_queue_, audio_divider),
			mixer_(sn76489_, opll_),
			speaker_(mixer_),
			keyboard_({Inputs::Keyboard::Key::Enter, Inputs::Keyboard::Key::Escape}, {}) {
			// Pick the clock rate based on the region.
			const double clock_rate = target.region == Target::Region::Europe ? 3546893.0 : 3579540.0;
			speaker_.set_input_rate(float(clock_rate / audio_divider));
			set_clock_rate(clock_rate);

			// Instantiate the joysticks.
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);

			// Clear the memory map.
			map(read_pointers_, nullptr, 0x10000, 0);
			map(write_pointers_, nullptr, 0x10000, 0);

			// Take a copy of the cartridge and place it into memory.
			if(!target.media.cartridges.empty()) {
				cartridge_ = target.media.cartridges[0]->get_segments()[0].data;
			}
			if(cartridge_.size() < 48*1024) {
				std::size_t new_space = 48*1024 - cartridge_.size();
				cartridge_.resize(48*1024);
				memset(&cartridge_[48*1024 - new_space], 0xff, new_space);
			}

			if(paging_scheme_ == Target::PagingScheme::Codemasters) {
				// The Codemasters cartridges start with pages 0, 1 and 0 again initially visible.
				paging_registers_[0] = 0;
				paging_registers_[1] = 1;
				paging_registers_[2] = 0;
			}

			// Load the BIOS if available.
			//
			// TODO: there's probably a million other versions of the Master System BIOS; try to build a
			// CRC32 catalogue of those. So far:
			//
			//	0072ed54 = US/European BIOS 1.3
			//	48d44a13 = Japanese BIOS 2.1
			const bool is_japanese = target.region == Target::Region::Japan;
			const ROM::Name bios_name = is_japanese ? ROM::Name::MasterSystemJapaneseBIOS : ROM::Name::MasterSystemWesternBIOS;
			ROM::Request request(bios_name, true);
			auto roms = rom_fetcher(request);
			request.validate(roms);

			const auto rom = roms.find(bios_name);
			if(rom == roms.end()) {
				// No BIOS found; attempt to boot as though it has already disabled itself.
				has_bios_ = false;
				memory_control_ |= 0x08;
				std::cerr << "No BIOS found; attempting to start cartridge directly" << std::endl;
			} else {
				has_bios_ = true;
				memcpy(&bios_, rom->second.data(), std::min(sizeof(bios_), rom->second.size()));
			}
			page_cartridge();

			// Map RAM.
			if constexpr (is_master_system(model)) {
				map(read_pointers_, ram_, 8*1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 8*1024, 0xc000, 0x10000);
			} else {
				map(read_pointers_, ram_, 1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 1024, 0xc000, 0x10000);
			}

			// Apply a relatively low low-pass filter. More guidance needed here.
			// TODO: this is disabled for now since it isn't applicable for the FM chip, I think.
//			speaker_.set_high_frequency_cutoff(8000);

			// Set default mixer levels: FM off, SN full-throttle.
			set_mixer_levels(0);

			keyboard_.set_delegate(this);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			vdp_->set_tv_standard(
				(region_ == Target::Region::Europe) ?
					TI::TMS::TVStandard::PAL : TI::TMS::TVStandard::NTSC);
			time_until_debounce_ = vdp_->get_time_until_line(-1);

			vdp_->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return vdp_->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			vdp_->set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() const final {
			return vdp_->get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		void run_for(const Cycles cycles) final {
			z80_.run_for(cycles);
		}

		void flush_output(int outputs) final {
			if(outputs & Output::Video) {
				vdp_.flush();
			}
			if(outputs & Output::Audio) {
				update_audio();
				audio_queue_.perform();
			}
		}

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			if(vdp_ += cycle.length) {
				z80_.set_interrupt_line(vdp_->get_interrupt_line(), vdp_.last_sequence_point_overrun());
			}
			time_since_sn76489_update_ += cycle.length;

			if(cycle.is_terminal()) {
				uint16_t address = cycle.address ? *cycle.address : 0x0000;
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
					case CPU::Z80::PartialMachineCycle::Read:
						*cycle.value = read_pointers_[address >> 10] ? read_pointers_[address >> 10][address & 1023] : 0xff;
					break;

					case CPU::Z80::PartialMachineCycle::Write:
						if(paging_scheme_ == Target::PagingScheme::Sega) {
							if(address >= 0xfffd && cartridge_.size() > 48*1024) {
								if(paging_registers_[address - 0xfffd] != *cycle.value) {
									paging_registers_[address - 0xfffd] = *cycle.value;
									page_cartridge();
								}
							}
						} else {
							// i.e. this is the Codemasters paging scheme.
							if(!(address&0x3fff) && address < 0xc000) {
								if(paging_registers_[address >> 14] != *cycle.value) {
									paging_registers_[address >> 14] = *cycle.value;
									page_cartridge();
								}
							}
						}

						if(write_pointers_[address >> 10]) write_pointers_[address >> 10][address & 1023] = *cycle.value;
//						else LOG("Ignored write to ROM");
					break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch(address & 0xc1) {
							case 0x00:
								LOG("TODO: [input] memory control");
								*cycle.value = 0xff;
							break;
							case 0x01:
								LOG("TODO: [input] I/O port control");
								*cycle.value = 0xff;
							break;
							case 0x40:
								*cycle.value = vdp_->get_current_line();
							break;
							case 0x41:
								*cycle.value = vdp_.last_valid()->get_latched_horizontal_counter();
							break;
							case 0x80: case 0x81:
								*cycle.value = vdp_->read(address);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
							break;
							case 0xc0: {
								if(memory_control_ & 0x4) {
									if(has_fm_audio_ && (address & 0xff) == 0xf2) {
										*cycle.value = opll_detection_word_;
									} else {
										*cycle.value = 0xff;
									}
								} else {
									Joystick *const joypad1 = static_cast<Joystick *>(joysticks_[0].get());
									Joystick *const joypad2 = static_cast<Joystick *>(joysticks_[1].get());
									*cycle.value = uint8_t(joypad1->get_state() | (joypad2->get_state() << 6));
								}
							} break;
							case 0xc1: {
								if(memory_control_ & 0x4) {
									*cycle.value = 0xff;
								} else {
									Joystick *const joypad2 = static_cast<Joystick *>(joysticks_[1].get());

									*cycle.value =
										(joypad2->get_state() >> 2) |
										0x30 |
										get_th_values();
								}
							} break;

							default:
								ERROR("[input] Clearly some sort of typo");
							break;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Output:
						switch(address & 0xc1) {
							case 0x00:		// i.e. even ports less than 0x40.
								if constexpr (is_master_system(model)) {
									// TODO: Obey the RAM enable.
									LOG("Memory control: " << PADHEX(2) << memory_control_);
									memory_control_ = *cycle.value;
									page_cartridge();
								}
							break;
							case 0x01: {	// i.e. odd ports less than 0x40.
								// A programmer can force the TH lines to 0 here,
								// causing a phoney lightgun latch, so check for any
								// discontinuity in TH inputs.
								const auto previous_ths = get_th_values();
								io_port_control_ = *cycle.value;
								const auto new_ths = get_th_values();

								// Latch if either TH has newly gone to 1.
								if((new_ths^previous_ths)&new_ths) {
									vdp_->latch_horizontal_counter();
								}
							} break;
							case 0x40: case 0x41:	// i.e. ports 0x40–0x7f.
								update_audio();
								sn76489_.write(*cycle.value);
							break;
							case 0x80: case 0x81:	// i.e. ports 0x80–0xbf.
								vdp_->write(address, *cycle.value);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
							break;
							case 0xc1: case 0xc0:	// i.e. ports 0xc0–0xff.
								if(has_fm_audio_) {
									switch(address & 0xff) {
										case 0xf0: case 0xf1:
											update_audio();
											opll_.write(address, *cycle.value);
										break;
										case 0xf2:
											opll_detection_word_ = *cycle.value;
											set_mixer_levels(opll_detection_word_);
										break;
									}
								}
							break;

							default:
								ERROR("[output] Clearly some sort of typo");
							break;
						}
					break;

/*
	TODO: implementation of the below is incomplete.
	Re: io_port_control_

	Set the TH pins for ports A and B as outputs. Set their output level
	to any value desired by writing to bits 7 and 5. Read the state of both
	TH pins back through bits 7 and 6 of port $DD. If the data returned is
	the same as the data written, it's an export machine, otherwise it's
	a domestic one.

	— Charles MacDonald
 */

					case CPU::Z80::PartialMachineCycle::Interrupt:
						*cycle.value = 0xff;
					break;

					default: break;
				}
			}

			// The pause button is debounced and takes effect only one line before pixels
			// begin; time_until_debounce_ keeps track of the time until then.
			time_until_debounce_ -= cycle.length;
			if(time_until_debounce_ <= HalfCycles(0)) {
				z80_.set_non_maskable_interrupt_line(pause_is_pressed_);
				time_until_debounce_ = vdp_->get_time_until_line(-1);
			}

			return HalfCycles(0);
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return joysticks_;
		}

		// MARK: - Keyboard (i.e. the pause and reset buttons).
		Inputs::Keyboard &get_keyboard() final {
			return keyboard_;
		}

		bool keyboard_did_change_key(Inputs::Keyboard *, Inputs::Keyboard::Key key, bool is_pressed) final {
			if(key == Inputs::Keyboard::Key::Enter) {
				pause_is_pressed_ = is_pressed;
				return true;
			}

			if(key == Inputs::Keyboard::Key::Escape) {
				reset_is_pressed_ = is_pressed;
				return true;
			}

			return false;
		}

		void reset_all_keys(Inputs::Keyboard *) final {
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
		}

	private:
		static constexpr TI::TMS::Personality tms_personality() {
			switch(model) {
				default:
				case Target::Model::SG1000:			return TI::TMS::TMS9918A;
				case Target::Model::MasterSystem:	return TI::TMS::SMSVDP;
				case Target::Model::MasterSystem2:	return TI::TMS::SMS2VDP;
			}
		}

		inline uint8_t get_th_values() {
			// Quick not on TH inputs here: if either is setup as an output, then the
			// currently output level is returned. Otherwise they're fixed at 1.
			return
				uint8_t(
					((io_port_control_ & 0x02) << 5) | ((io_port_control_&0x20) << 1) |
					((io_port_control_ & 0x08) << 4) | (io_port_control_&0x80)
				);

		}

		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(audio_divider)));
		}

		void set_mixer_levels(uint8_t mode) {
			// This is as per the audio control register;
			// see https://www.smspower.org/Development/AudioControlPort
			update_audio();
			audio_queue_.enqueue([this, mode] {
				switch(mode & 3) {
					case 0:	// SN76489 only; the default.
						mixer_.set_relative_volumes({1.0f, 0.0f});
					break;

					case 1: // FM only.
						mixer_.set_relative_volumes({0.0f, 1.0f});
					break;

					case 2: // No audio.
						mixer_.set_relative_volumes({0.0f, 0.0f});
					break;

					case 3: // Both FM and SN76489.
						mixer_.set_relative_volumes({0.5f, 0.5f});
					break;
				}
			});
		}

		using Target = Analyser::Static::Sega::Target;
		const Target::Region region_;
		const Target::PagingScheme paging_scheme_;
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		JustInTimeActor<TI::TMS::TMS9918<tms_personality()>> vdp_;

		Concurrency::AsyncTaskQueue<false> audio_queue_;
		TI::SN76489 sn76489_;
		Yamaha::OPL::OPLL opll_;
		Outputs::Speaker::CompoundSource<decltype(sn76489_), decltype(opll_)> mixer_;
		Outputs::Speaker::PullLowpass<decltype(mixer_)> speaker_;
		uint8_t opll_detection_word_ = 0xff;

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		Inputs::Keyboard keyboard_;
		bool reset_is_pressed_ = false, pause_is_pressed_ = false;

		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_debounce_;

		uint8_t ram_[8*1024];
		uint8_t bios_[8*1024];
		std::vector<uint8_t> cartridge_;

		uint8_t io_port_control_ = 0x0f;

		// This is a static constexpr for now; I may use it in the future.
		static constexpr bool has_fm_audio_ = true;

		// The memory map has a 1kb granularity; this is determined by the SG1000's 1kb of RAM.
		const uint8_t *read_pointers_[64];
		uint8_t *write_pointers_[64];
		template <typename T> void map(T **target, uint8_t *source, size_t size, size_t start_address, size_t end_address = 0) {
			if(!end_address) end_address = start_address + size;
			for(auto address = start_address; address < end_address; address += 1024) {
				target[address >> 10] = source ? &source[(address - start_address) & (size - 1)] : nullptr;
			}
		}

		uint8_t paging_registers_[3] = {0, 1, 2};
		uint8_t memory_control_ = 0;
		void page_cartridge() {
			// Either install the cartridge or don't; Japanese machines can't see
			// anything but the cartridge.
			if(!(memory_control_ & 0x40) || region_ == Target::Region::Japan) {
				for(size_t c = 0; c < 3; ++c) {
					const size_t start_addr = (paging_registers_[c] * 0x4000) % cartridge_.size();
					map(
						read_pointers_,
						cartridge_.data() + start_addr,
						std::min(size_t(0x4000), cartridge_.size() - start_addr),
						c * 0x4000);
				}

				// The first 1kb doesn't page though, if this is the Sega paging scheme.
				if(paging_scheme_ == Target::PagingScheme::Sega) {
					map(read_pointers_, cartridge_.data(), 0x400, 0x0000);
				}
			} else {
				map(read_pointers_, nullptr, 0xc000, 0x0000);
			}

			// Throw the BIOS on top if this machine has one and it isn't disabled.
			if(has_bios_ && !(memory_control_ & 0x08)) {
				map(read_pointers_, bios_, 8*1024, 0);
			}
		}
		bool has_bios_ = true;
};

}
}

using namespace Sega::MasterSystem;

Machine *Machine::MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Sega::Target;
	const Target *const sega_target = dynamic_cast<const Target *>(target);

	switch(sega_target->model) {
		case Target::Model::SG1000:			return new ConcreteMachine<Target::Model::SG1000>(*sega_target, rom_fetcher);
		case Target::Model::MasterSystem:	return new ConcreteMachine<Target::Model::MasterSystem>(*sega_target, rom_fetcher);
		case Target::Model::MasterSystem2:	return new ConcreteMachine<Target::Model::MasterSystem2>(*sega_target, rom_fetcher);
		default:
			assert(false);
	}
}

Machine::~Machine() {}
