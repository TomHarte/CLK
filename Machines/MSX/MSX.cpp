//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include "Keyboard.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/1770/1770.hpp"
#include "../../Components/9918/9918.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../CRTMachine.hpp"
#include "../ConfigurationTarget.hpp"
#include "../KeyboardMachine.hpp"

#include "../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace MSX {

/*!
	Provides a sample source that can programmatically be set to one of two values.
*/
class AudioToggle: public Outputs::Speaker::SampleSource {
	public:
		AudioToggle(Concurrency::DeferringAsyncTaskQueue &audio_queue) :
			audio_queue_(audio_queue) {}

		void get_samples(std::size_t number_of_samples, std::int16_t *target) {
			for(std::size_t sample = 0; sample < number_of_samples; ++sample) {
				target[sample] = level_;
			}
		}

		void skip_samples(const std::size_t number_of_samples) {}

		void set_output(bool enabled) {
			if(is_enabled_ == enabled) return;
			is_enabled_ = enabled;
			audio_queue_.defer([=] {
				level_ = enabled ? 4096 : 0;
			});
		}

		bool get_output() {
			return is_enabled_;
		}

	private:
		bool is_enabled_ = false;
		int16_t level_ = 0;
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;
};

struct AYPortHandler: public GI::AY38910::PortHandler {
	void set_port_output(bool port_b, uint8_t value) {
//		printf("AY port %c output: %02x\n", port_b ? 'b' : 'a', value);
	}

	uint8_t get_port_input(bool port_b) {
//		printf("AY port %c input\n", port_b ? 'b' : 'a');
		return 0xff;
	}
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine {
	public:
		ConcreteMachine():
			z80_(*this),
			i8255_(i8255_port_handler_),
			ay_(audio_queue_),
			audio_toggle_(audio_queue_),
			mixer_(ay_, audio_toggle_),
			speaker_(mixer_),
			i8255_port_handler_(*this, audio_toggle_) {
			set_clock_rate(3579545);
			std::memset(unpopulated_, 0xff, sizeof(unpopulated_));
			clear_all_keys();

			ay_.set_port_handler(&ay_port_handler_);
			speaker_.set_input_rate(3579545.0f / 2.0f);
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS9918(TI::TMS9918::TMS9918A));
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

		void configure_as_target(const StaticAnalyser::Target &target) override {
			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override {
			if(!media.cartridges.empty()) {
				const auto &segment = media.cartridges.front()->get_segments().front();
				cartridge_ = segment.data;

				// TODO: should clear other page 1 pointers, should allow for paging cartridges, etc.
				size_t base = segment.start_address >> 14;
				for(size_t c = 0; c < cartridge_.size(); c += 16384) {
					memory_slots_[1].read_pointers[(c >> 14) + base] = cartridge_.data() + c;
				}
			}
			return true;
		}

		void page_memory(uint8_t value) {
			for(size_t c = 0; c < 4; ++c) {
				read_pointers_[c] = memory_slots_[value & 3].read_pointers[c];
				write_pointers_[c] = memory_slots_[value & 3].write_pointers[c];
				value >>= 2;
			}
		}

		HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_interrupt_line(true, time_until_interrupt_);
				}
			}

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					*cycle.value = read_pointers_[address >> 14][address & 16383];
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					write_pointers_[address >> 14][address & 16383] = *cycle.value;
				break;

				case CPU::Z80::PartialMachineCycle::Input:
					switch(address & 0xff) {
						case 0x98:	case 0x99:
							vdp_->run_for(time_since_vdp_update_.flush());
							*cycle.value = vdp_->get_register(address);
							z80_.set_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						case 0xa2:
							update_audio();
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(GI::AY38910::BC2 | GI::AY38910::BC1));
							*cycle.value = ay_.get_data_output();
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(0));
						break;

						case 0xa8:	case 0xa9:
						case 0xaa:	case 0xab:
							*cycle.value = i8255_.get_register(address);
						break;

						default:
							*cycle.value = 0xff;
						break;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Output: {
					const int port = address & 0xff;
					switch(port) {
						case 0x98:	case 0x99:
							vdp_->run_for(time_since_vdp_update_.flush());
							vdp_->set_register(address, *cycle.value);
							z80_.set_interrupt_line(vdp_->get_interrupt_line());
							time_until_interrupt_ = vdp_->get_time_until_interrupt();
						break;

						case 0xa0:	case 0xa1:
							update_audio();
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(GI::AY38910::BDIR | GI::AY38910::BC2 | ((port == 0xa0) ? GI::AY38910::BC1 : 0)));
							ay_.set_data_input(*cycle.value);
							ay_.set_control_lines(static_cast<GI::AY38910::ControlLines>(0));
						break;

						case 0xa8:	case 0xa9:
						case 0xaa:	case 0xab:
							i8255_.set_register(address, *cycle.value);
						break;

						case 0xfc: case 0xfd: case 0xfe: case 0xff:
//							printf("RAM banking %02x: %02x\n", port, *cycle.value);
						break;
					}
				} break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					*cycle.value = 0xff;
				break;

				default: break;
			}

			// Per the best information I currently have, the MSX inserts an extra cycle into each opcode read,
			// but otherwise runs without pause.
			HalfCycles addition((cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) ? 2 : 0);
			time_since_vdp_update_ += cycle.length + addition;
			time_since_ay_update_ += cycle.length + addition;
			return addition;
		}

		void flush() {
			vdp_->run_for(time_since_vdp_update_.flush());
			update_audio();
			audio_queue_.perform();
		}

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"MSX",
				{
					"msx.rom"
				});

			if(!roms[0]) return false;

			rom_ = std::move(*roms[0]);
			rom_.resize(32768);

			for(size_t c = 0; c < 4; ++c) {
				for(size_t slot = 0; slot < 3; ++slot) {
					memory_slots_[slot].read_pointers[c] = unpopulated_;
					memory_slots_[slot].write_pointers[c] = scratch_;
				}

				memory_slots_[3].read_pointers[c] =
				memory_slots_[3].write_pointers[c] = &ram_[c * 16384];
			}

			memory_slots_[0].read_pointers[0] = rom_.data();
			memory_slots_[0].read_pointers[1] = &rom_[16384];

			for(size_t c = 0; c < 4; ++c) {
				read_pointers_[c] = memory_slots_[0].read_pointers[c];
				write_pointers_[c] = memory_slots_[0].write_pointers[c];
			}

			return true;
		}

		void set_keyboard_line(int line) {
			selected_key_line_ = line;
		}

		uint8_t read_keyboard() {
			return key_states_[selected_key_line_];
		}

		void clear_all_keys() override {
			std::memset(key_states_, 0xff, sizeof(key_states_));
		}

		void set_key_state(uint16_t key, bool is_pressed) override {
			int mask = 1 << (key & 7);
			int line = key >> 4;
			if(is_pressed) key_states_[line] &= ~mask; else key_states_[line] |= mask;
		}

		KeyboardMapper &get_keyboard_mapper() override {
			return keyboard_mapper_;
		}

	private:
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_ay_update_.divide_cycles(Cycles(2)));
		}

		class i8255PortHandler: public Intel::i8255::PortHandler {
			public:
				i8255PortHandler(ConcreteMachine &machine, AudioToggle &audio_toggle) :
					machine_(machine), audio_toggle_(audio_toggle) {}

				void set_value(int port, uint8_t value) {
					switch(port) {
						case 0:	machine_.page_memory(value);	break;
						case 2: {
							// TODO:
							//	b6	caps lock LED
							//	b5 	audio output
							//	b4	cassette motor relay

							//	b7: keyboard click
							bool new_audio_level = !!(value & 0x80);
							if(audio_toggle_.get_output() != new_audio_level) {
								machine_.update_audio();
								audio_toggle_.set_output(new_audio_level);
							}

							// b0–b3: keyboard line
							machine_.set_keyboard_line(value & 0xf);
						} break;
						default: printf("What what what what?\n"); break;
					}
				}

				uint8_t get_value(int port) {
					if(port == 1) {
						return machine_.read_keyboard();
					} else printf("What what?\n");
					return 0xff;
				}

			private:
				ConcreteMachine &machine_;
				AudioToggle &audio_toggle_;
		};

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910 ay_;
		AudioToggle audio_toggle_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910, AudioToggle> mixer_;
		Outputs::Speaker::LowpassSpeaker<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910, AudioToggle>> speaker_;

		i8255PortHandler i8255_port_handler_;
		AYPortHandler ay_port_handler_;

		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];

		struct MemorySlots {
			uint8_t *read_pointers[4];
			uint8_t *write_pointers[4];
		} memory_slots_[4];

		uint8_t ram_[65536];
		uint8_t scratch_[16384];
		uint8_t unpopulated_[16384];
		std::vector<uint8_t> rom_;
		std::vector<uint8_t> cartridge_;

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_ay_update_;
		HalfCycles time_until_interrupt_;

		uint8_t key_states_[16];
		int selected_key_line_ = 0;

		MSX::KeyboardMapper keyboard_mapper_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
