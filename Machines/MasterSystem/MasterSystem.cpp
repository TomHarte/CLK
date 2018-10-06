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

#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Analyser/Static/Sega/Target.hpp"

#include <algorithm>

namespace {
const int sn76489_divider = 2;
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

		void did_set_input(const Input &digital_input, bool is_active) override {
			switch(digital_input.type) {
				default: return;

				case Input::Up: 	if(is_active) state_ &= ~0x01; else state_ |= 0x01;	break;
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

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public JoystickMachine::Machine {

	public:
		ConcreteMachine(const Analyser::Static::Sega::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			model_(target.model),
			z80_(*this),
			sn76489_(
				(target.model == Analyser::Static::Sega::Target::Model::SG1000) ? TI::SN76489::Personality::SN76489 : TI::SN76489::Personality::SMS,
				audio_queue_,
				sn76489_divider),
			speaker_(sn76489_) {
			speaker_.set_input_rate(3579545.0f / static_cast<float>(sn76489_divider));
			set_clock_rate(3579545);

			// Instantiate the joysticks.
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);

			// Clear the memory map.
			map(read_pointers_, nullptr, 0x10000, 0);
			map(write_pointers_, nullptr, 0x10000, 0);

			// Take a copy of the cartridge and place it into memory.
			cartridge_ = target.media.cartridges[0]->get_segments()[0].data;
			if(cartridge_.size() < 48*1024) {
				std::size_t new_space = 48*1024 - cartridge_.size();
				cartridge_.resize(48*1024);
				memset(&cartridge_[48*1024 - new_space], 0xff, new_space);
			}
			page_cartridge();

			// Establish the BIOS (if relevant) and RAM.
			if(target.model == Analyser::Static::Sega::Target::Model::MasterSystem) {
				const auto roms = rom_fetcher("MasterSystem", {"bios.sms"});
				if(!roms[0]) {
					throw ROMMachine::Error::MissingROMs;
				}

				roms[0]->resize(8*1024);
				memcpy(&bios_, roms[0]->data(), roms[0]->size());
				map(read_pointers_, bios_, 8*1024, 0);

				map(read_pointers_, ram_, 8*1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 8*1024, 0xc000, 0x10000);
			} else {
				map(read_pointers_, ram_, 1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 1024, 0xc000, 0x10000);
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS::TMS9918(model_ == Analyser::Static::Sega::Target::Model::SG1000 ? TI::TMS::TMS9918A : TI::TMS::SMSVDP));
//			get_crt()->set_video_signal(Outputs::CRT::VideoSignal::Composite);
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

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			time_since_vdp_update_ += cycle.length;
			time_since_sn76489_update_ += cycle.length;

			if(cycle.is_terminal()) {
				uint16_t address = cycle.address ? *cycle.address : 0x0000;
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
					case CPU::Z80::PartialMachineCycle::Read:
						*cycle.value = read_pointers_[address >> 10] ? read_pointers_[address >> 10][address & 1023] : 0xff;
					break;

					case CPU::Z80::PartialMachineCycle::Write:
						if(address >= 0xfffd && cartridge_.size() > 48*1024) {
							if(paging_registers_[address - 0xfffd] != *cycle.value) {
								paging_registers_[address - 0xfffd] = *cycle.value;
								page_cartridge();
							}
						}

						if(write_pointers_[address >> 10]) write_pointers_[address >> 10][address & 1023] = *cycle.value;
					break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch(address & 0xc1) {
							case 0x00:
								printf("TODO: [input] memory control\n");
								*cycle.value = 0xff;
							break;
							case 0x01:
								printf("TODO: [input] I/O port control\n");
								*cycle.value = 0xff;
							break;
							case 0x40: case 0x41:
								update_video();
								*cycle.value = vdp_->get_current_line();
							break;
							case 0x80: case 0x81:
								update_video();
								*cycle.value = vdp_->get_register(address);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;
							case 0xc0: {
								Joystick *const joypad1 = static_cast<Joystick *>(joysticks_[0].get());
								Joystick *const joypad2 = static_cast<Joystick *>(joysticks_[1].get());
								*cycle.value = static_cast<uint8_t>(joypad1->get_state() | (joypad2->get_state() << 6));
							} break;
							case 0xc1: {
								Joystick *const joypad2 = static_cast<Joystick *>(joysticks_[1].get());
								*cycle.value = (joypad2->get_state() >> 2) | 0xf;
							} break;

							default:
								printf("[input] Clearly some sort of typo\n");
							break;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Output:
						switch(address & 0xc1) {
							case 0x00:
								if(model_ == Analyser::Static::Sega::Target::Model::MasterSystem) {
									// TODO: Obey the RAM enable.
									memory_control_ = *cycle.value;
									page_cartridge();
								}
							break;
							case 0x01:
								printf("TODO: [output] I/O port control\n");
							break;
							case 0x40: case 0x41:
								update_audio();
								sn76489_.set_register(*cycle.value);
							break;
							case 0x80: case 0x81:
								update_video();
								vdp_->set_register(address, *cycle.value);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;
							case 0xc0:
//								printf("TODO: [output] I/O port A/N [%02x]\n", *cycle.value);
							break;
							case 0xc1:
								printf("TODO: [output] I/O port B/misc\n");
							break;

							default:
								printf("[output] Clearly some sort of typo\n");
							break;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Interrupt:
						*cycle.value = 0xff;
					break;

					default: break;
				}
			}

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_interrupt_line(true, time_until_interrupt_);
				}
			}

			return HalfCycles(0);
		}

		void flush() {
			update_video();
			update_audio();
			audio_queue_.perform();
		}

		std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}

	private:
		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(sn76489_divider)));
		}
		inline void update_video() {
			vdp_->run_for(time_since_vdp_update_.flush());
		}

		Analyser::Static::Sega::Target::Model model_;
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS::TMS9918> vdp_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TI::SN76489 sn76489_;
		Outputs::Speaker::LowpassSpeaker<TI::SN76489> speaker_;

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_interrupt_;

		uint8_t ram_[8*1024];
		uint8_t bios_[8*1024];
		std::vector<uint8_t> cartridge_;

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
			// Either install the cartridge or don't.
			if(!(memory_control_ & 0x40)) {
				for(size_t c = 0; c < 3; ++c) {
					const size_t start_addr = (paging_registers_[c] * 0x4000) % cartridge_.size();
					map(
						read_pointers_,
						cartridge_.data() + start_addr,
						std::min(static_cast<size_t>(0x4000), cartridge_.size() - start_addr),
						c * 0x4000);
				}

				// The first 1kb doesn't page though.
				map(read_pointers_, cartridge_.data(), 0x400, 0x0000);
			} else {
				map(read_pointers_, nullptr, 0xc000, 0x0000);
			}

			// Throw the BIOS on top if it isn't disabled.
			if(!(memory_control_ & 0x08)) {
				map(read_pointers_, bios_, 8*1024, 0);
			}
		}
};

}
}

using namespace Sega::MasterSystem;

Machine *Machine::MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Sega::Target;
	const Target *const sega_target = dynamic_cast<const Target *>(target);
	return new ConcreteMachine(*sega_target, rom_fetcher);
}

Machine::~Machine() {}
