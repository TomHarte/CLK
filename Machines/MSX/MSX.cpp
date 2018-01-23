//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include "DiskROM.hpp"
#include "Keyboard.hpp"
#include "ROMSlotHandler.hpp"

#include "Cartridges/ASCII8kb.hpp"
#include "Cartridges/ASCII16kb.hpp"
#include "Cartridges/Konami.hpp"
#include "Cartridges/KonamiWithSCC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/1770/1770.hpp"
#include "../../Components/9918/9918.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/KonamiSCC/KonamiSCC.hpp"

#include "../../Storage/Tape/Parsers/MSX.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../CRTMachine.hpp"
#include "../ConfigurationTarget.hpp"
#include "../KeyboardMachine.hpp"

#include "../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

#include "../../Configurable/StandardOptions.hpp"

namespace MSX {

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(Configurable::DisplayRGBComposite | Configurable::QuickLoadTape)
	);
}

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

class AYPortHandler: public GI::AY38910::PortHandler {
	public:
		AYPortHandler(Storage::Tape::BinaryTapePlayer &tape_player) : tape_player_(tape_player) {}

		void set_port_output(bool port_b, uint8_t value) {
			if(port_b) {
				// Bits 0–3: touchpad handshaking (?)
				// Bit 4—5: monostable timer pulses
				// Bit 6: joystick select
				// Bit 7: code LED, if any
			}
		}

		uint8_t get_port_input(bool port_b) {
			if(!port_b) {
				// Bits 0–5: Joystick (up, down, left, right, A, B)
				// Bit 6: keyboard switch (not universal)

				// Bit 7: tape input
				return 0x7f | (tape_player_.get_input() ? 0x00 : 0x80);
			}
			return 0xff;
		}

	private:
		Storage::Tape::BinaryTapePlayer &tape_player_;
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine,
	public Configurable::Device,
	public MemoryMap {
	public:
		ConcreteMachine():
			z80_(*this),
			i8255_(i8255_port_handler_),
			ay_(audio_queue_),
			audio_toggle_(audio_queue_),
			scc_(audio_queue_),
			mixer_(ay_, audio_toggle_, scc_),
			speaker_(mixer_),
			tape_player_(3579545 * 2),
			i8255_port_handler_(*this, audio_toggle_, tape_player_),
			ay_port_handler_(tape_player_) {
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

			if(memory_slots_[1].handler) {
				printf("%0.2f\n", memory_slots_[1].handler->get_confidence());
			}
		}

		void configure_as_target(const StaticAnalyser::Target &target) override {
			// Add a disk cartridge if any disks were supplied.
			if(!target.media.disks.empty()) {
				map(2, 0, 0x4000, 0x2000);
				unmap(2, 0x6000, 0x2000);
				memory_slots_[2].set_handler(new DiskROM(memory_slots_[2].source));
			}

			// Insert the media.
			insert_media(target.media);

			// Type whatever has been requested.
			if(target.loading_command.length()) {
				type_string(target.loading_command);
			}

			// Attach the hardware necessary for a game cartridge, if any.
			switch(target.msx.cartridge_type) {
				default: break;
				case StaticAnalyser::MSXCartridgeType::Konami:
					memory_slots_[1].set_handler(new Cartridge::KonamiROMSlotHandler(*this, 1));
				break;
				case StaticAnalyser::MSXCartridgeType::KonamiWithSCC:
					memory_slots_[1].set_handler(new Cartridge::KonamiWithSCCROMSlotHandler(*this, 1, scc_));
				break;
				case StaticAnalyser::MSXCartridgeType::ASCII8kb:
					memory_slots_[1].set_handler(new Cartridge::ASCII8kbROMSlotHandler(*this, 1));
				break;
				case StaticAnalyser::MSXCartridgeType::ASCII16kb:
					memory_slots_[1].set_handler(new Cartridge::ASCII16kbROMSlotHandler(*this, 1));
				break;
			}
		}

		bool insert_media(const StaticAnalyser::Media &media) override {
			if(!media.cartridges.empty()) {
				const auto &segment = media.cartridges.front()->get_segments().front();
				memory_slots_[1].source = segment.data;
				map(1, 0, static_cast<uint16_t>(segment.start_address), std::min(segment.data.size(), 65536 - segment.start_address));
			}

			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
			}

			if(!media.disks.empty()) {
				DiskROM *disk_rom = dynamic_cast<DiskROM *>(memory_slots_[2].handler.get());
				int drive = 0;
				for(auto &disk : media.disks) {
					disk_rom->set_disk(disk, drive);
					drive++;
					if(drive == 2) break;
				}
			}

			return true;
		}

		void type_string(const std::string &string) override final {
			input_text_ += string;
		}

		// MARK: MSX::MemoryMap
		void map(int slot, std::size_t source_address, uint16_t destination_address, std::size_t length) override {
			assert(!(destination_address & 8191));
			assert(!(length & 8191));
			assert(static_cast<std::size_t>(destination_address) + length <= 65536);

			for(std::size_t c = 0; c < (length >> 13); ++c) {
				if(memory_slots_[slot].wrapping_strategy == ROMSlotHandler::WrappingStrategy::Repeat) source_address %= memory_slots_[slot].source.size();
				memory_slots_[slot].read_pointers[(destination_address >> 13) + c] =
					(source_address < memory_slots_[slot].source.size()) ? &memory_slots_[slot].source[source_address] : unpopulated_;
				source_address += 8192;
			}

			page_memory(paged_memory_);
		}

		void unmap(int slot, uint16_t destination_address, std::size_t length) override {
			assert(!(destination_address & 8191));
			assert(!(length & 8191));
			assert(static_cast<std::size_t>(destination_address) + length <= 65536);

			for(std::size_t c = 0; c < (length >> 13); ++c) {
				memory_slots_[slot].read_pointers[(destination_address >> 13) + c] = nullptr;
			}

			page_memory(paged_memory_);
		}

		// MARK: Ordinary paging.
		void page_memory(uint8_t value) {
			paged_memory_ = value;
			for(std::size_t c = 0; c < 8; c += 2) {
				read_pointers_[c] = memory_slots_[value & 3].read_pointers[c];
				write_pointers_[c] = memory_slots_[value & 3].write_pointers[c];
				read_pointers_[c+1] = memory_slots_[value & 3].read_pointers[c+1];
				write_pointers_[c+1] = memory_slots_[value & 3].write_pointers[c+1];
				value >>= 2;
			}
		}

		// MARK: Z80::BusHandler
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
					if(use_fast_tape_ && tape_player_.has_tape()) {
						if(address == 0x1a63) {
							// TAPION

							// Enable the tape motor.
							i8255_.set_register(0xab, 0x8);

							// Disable interrupts.
							z80_.set_value_of_register(CPU::Z80::Register::IFF1, 0);
							z80_.set_value_of_register(CPU::Z80::Register::IFF2, 0);

							// Use the parser to find a header, and if one is found then populate
							// LOWLIM and WINWID, and reset carry. Otherwise set carry.
							using Parser = Storage::Tape::MSX::Parser;
							std::unique_ptr<Parser::FileSpeed> new_speed = Parser::find_header(tape_player_);
							if(new_speed) {
								ram_[0xfca4] = new_speed->minimum_start_bit_duration;
								ram_[0xfca5] = new_speed->low_high_disrimination_duration;
								z80_.set_value_of_register(CPU::Z80::Register::Flags, 0);
							} else {
								z80_.set_value_of_register(CPU::Z80::Register::Flags, 1);
							}

							// RET.
							*cycle.value = 0xc9;
							break;
						}

						if(address == 0x1abc) {
							// TAPIN

							// Grab the current values of LOWLIM and WINWID.
							using Parser = Storage::Tape::MSX::Parser;
							Parser::FileSpeed tape_speed;
							tape_speed.minimum_start_bit_duration = ram_[0xfca4];
							tape_speed.low_high_disrimination_duration = ram_[0xfca5];

							// Ask the tape parser to grab a byte.
							int next_byte = Parser::get_byte(tape_speed, tape_player_);

							// If a byte was found, return it with carry unset. Otherwise set carry to
							// indicate error.
							if(next_byte >= 0) {
								z80_.set_value_of_register(CPU::Z80::Register::A, static_cast<uint16_t>(next_byte));
								z80_.set_value_of_register(CPU::Z80::Register::Flags, 0);
							} else {
								z80_.set_value_of_register(CPU::Z80::Register::Flags, 1);
							}

							// RET.
							*cycle.value = 0xc9;
							break;
						}
					}
				case CPU::Z80::PartialMachineCycle::Read:
					if(read_pointers_[address >> 13]) {
						*cycle.value = read_pointers_[address >> 13][address & 8191];
					} else {
						int slot_hit = (paged_memory_ >> ((address >> 14) * 2)) & 3;
						memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.flush());
						*cycle.value = memory_slots_[slot_hit].handler->read(address);
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write: {
					write_pointers_[address >> 13][address & 8191] = *cycle.value;

					int slot_hit = (paged_memory_ >> ((address >> 14) * 2)) & 3;
					if(memory_slots_[slot_hit].handler) {
						update_audio();
						memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.flush());
						memory_slots_[slot_hit].handler->write(address, *cycle.value);
					}
				} break;

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

					// Take this as a convenient moment to jump into the keyboard buffer, if desired.
					if(!input_text_.empty()) {
						// TODO: is it safe to assume these addresses?
						const int buffer_start = 0xfbf0;
						const int buffer_end = 0xfb18;

						int read_address = ram_[0xf3fa] | (ram_[0xf3fb] << 8);
						int write_address = ram_[0xf3f8] | (ram_[0xf3f9] << 8);

						const int buffer_size = buffer_end - buffer_start;
						int available_space = write_address + buffer_size - read_address - 1;

						const std::size_t characters_to_write = std::min(static_cast<std::size_t>(available_space), input_text_.size());
						write_address -= buffer_start;
						for(std::size_t c = 0; c < characters_to_write; ++c) {
							char character = input_text_[c];
							ram_[write_address + buffer_start] = static_cast<uint8_t>(character);
							write_address = (write_address + 1) % buffer_size;
						}
						write_address += buffer_start;
						input_text_.erase(input_text_.begin(), input_text_.begin() + static_cast<std::string::difference_type>(characters_to_write));

						ram_[0xf3f8] = static_cast<uint8_t>(write_address);
						ram_[0xf3f9] = static_cast<uint8_t>(write_address >> 8);
					}
				break;

				default: break;
			}

			// Update the tape. (TODO: allow for sleeping)
			tape_player_.run_for(cycle.length.as_int());

			// Per the best information I currently have, the MSX inserts an extra cycle into each opcode read,
			// but otherwise runs without pause.
			HalfCycles addition((cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) ? 2 : 0);
			time_since_vdp_update_ += cycle.length + addition;
			time_since_ay_update_ += cycle.length + addition;
			memory_slots_[0].cycles_since_update  += cycle.length + addition;
			memory_slots_[1].cycles_since_update  += cycle.length + addition;
			memory_slots_[2].cycles_since_update  += cycle.length + addition;
			memory_slots_[3].cycles_since_update  += cycle.length + addition;
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
					"msx.rom",
					"disk.rom"
				});

			if(!roms[0] || !roms[1]) return false;

			memory_slots_[0].source = std::move(*roms[0]);
			memory_slots_[0].source.resize(32768);

			memory_slots_[2].source = std::move(*roms[1]);
			memory_slots_[2].source.resize(16384);

			for(size_t c = 0; c < 8; ++c) {
				for(size_t slot = 0; slot < 3; ++slot) {
					memory_slots_[slot].read_pointers[c] = unpopulated_;
					memory_slots_[slot].write_pointers[c] = scratch_;
				}

				memory_slots_[3].read_pointers[c] =
				memory_slots_[3].write_pointers[c] = &ram_[c * 8192];
			}

			map(0, 0, 0, 32768);
			page_memory(0);

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

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return MSX::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				use_fast_tape_ = quickload;
			}

			Configurable::Display display;
			if(Configurable::get_display(selections_by_option, display)) {
				get_crt()->set_output_device((display == Configurable::Display::RGB) ? Outputs::CRT::OutputDevice::Monitor : Outputs::CRT::OutputDevice::Television);
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			Configurable::append_display_selection(selection_set, Configurable::Display::Composite);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			Configurable::append_display_selection(selection_set, Configurable::Display::RGB);
			return selection_set;
		}

	private:
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_ay_update_.divide_cycles(Cycles(2)));
		}

		class i8255PortHandler: public Intel::i8255::PortHandler {
			public:
				i8255PortHandler(ConcreteMachine &machine, AudioToggle &audio_toggle, Storage::Tape::BinaryTapePlayer &tape_player) :
					machine_(machine), audio_toggle_(audio_toggle), tape_player_(tape_player) {}

				void set_value(int port, uint8_t value) {
					switch(port) {
						case 0:	machine_.page_memory(value);	break;
						case 2: {
							// TODO:
							//	b6	caps lock LED
							//	b5 	audio output

							//	b4: cassette motor relay
							tape_player_.set_motor_control(!(value & 0x10));

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
				Storage::Tape::BinaryTapePlayer &tape_player_;
		};

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS9918> vdp_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910 ay_;
		AudioToggle audio_toggle_;
		Konami::SCC scc_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910, AudioToggle, Konami::SCC> mixer_;
		Outputs::Speaker::LowpassSpeaker<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910, AudioToggle, Konami::SCC>> speaker_;

		Storage::Tape::BinaryTapePlayer tape_player_;
		bool use_fast_tape_ = false;

		i8255PortHandler i8255_port_handler_;
		AYPortHandler ay_port_handler_;

		uint8_t paged_memory_ = 0;
		uint8_t *read_pointers_[8];
		uint8_t *write_pointers_[8];

		struct MemorySlots {
			uint8_t *read_pointers[8];
			uint8_t *write_pointers[8];

			void set_handler(ROMSlotHandler *slot_handler) {
				handler.reset(slot_handler);
				wrapping_strategy = handler->wrapping_strategy();
			}

			std::unique_ptr<ROMSlotHandler> handler;
			std::vector<uint8_t> source;
			HalfCycles cycles_since_update;
			ROMSlotHandler::WrappingStrategy wrapping_strategy = ROMSlotHandler::WrappingStrategy::Repeat;
		} memory_slots_[4];

		uint8_t ram_[65536];
		uint8_t scratch_[8192];
		uint8_t unpopulated_[8192];

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_ay_update_;
		HalfCycles time_until_interrupt_;

		uint8_t key_states_[16];
		int selected_key_line_ = 0;
		std::string input_text_;

		MSX::KeyboardMapper keyboard_mapper_;
};

}

using namespace MSX;

Machine *Machine::MSX() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
