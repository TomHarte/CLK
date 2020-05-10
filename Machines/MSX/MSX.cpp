//
//  MSX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MSX.hpp"

#include <algorithm>

#include "DiskROM.hpp"
#include "Keyboard.hpp"
#include "ROMSlotHandler.hpp"

#include "../../Analyser/Static/MSX/Cartridge.hpp"
#include "Cartridges/ASCII8kb.hpp"
#include "Cartridges/ASCII16kb.hpp"
#include "Cartridges/Konami.hpp"
#include "Cartridges/KonamiWithSCC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/1770/1770.hpp"
#include "../../Components/9918/9918.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AudioToggle/AudioToggle.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/KonamiSCC/KonamiSCC.hpp"

#include "../../Storage/Tape/Parsers/MSX.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../../Activity/Source.hpp"
#include "../MachineTypes.hpp"
#include "../../Configurable/Configurable.hpp"

#include "../../Outputs/Log.hpp"
#include "../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"

#include "../../Configurable/StandardOptions.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../ClockReceiver/JustInTime.hpp"

#include "../../Analyser/Static/MSX/Target.hpp"

namespace MSX {

class AYPortHandler: public GI::AY38910::PortHandler {
	public:
		AYPortHandler(Storage::Tape::BinaryTapePlayer &tape_player) : tape_player_(tape_player) {
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);
		}

		void set_port_output(bool port_b, uint8_t value) {
			if(port_b) {
				// Bits 0-3: touchpad handshaking (?)
				// Bit 4-5: monostable timer pulses

				// Bit 6: joystick select
				selected_joystick_ = (value >> 6) & 1;

				// Bit 7: code LED, if any
			}
		}

		uint8_t get_port_input(bool port_b) {
			if(!port_b) {
				// Bits 0-5: Joystick (up, down, left, right, A, B)
				// Bit 6: keyboard switch (not universal)
				// Bit 7: tape input
				return
					(static_cast<Joystick *>(joysticks_[selected_joystick_].get())->get_state() & 0x3f) |
					0x40 |
					(tape_player_.get_input() ? 0x00 : 0x80);
			}
			return 0xff;
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() {
			return joysticks_;
		}

	private:
		Storage::Tape::BinaryTapePlayer &tape_player_;

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		size_t selected_joystick_ = 0;
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
					}) {}

				void did_set_input(const Input &input, bool is_active) final {
					uint8_t mask = 0;
					switch(input.type) {
						default: return;
						case Input::Up:		mask = 0x01;	break;
						case Input::Down:	mask = 0x02;	break;
						case Input::Left:	mask = 0x04;	break;
						case Input::Right:	mask = 0x08;	break;
						case Input::Fire:
							if(input.info.control.index >= 2) return;
							mask = input.info.control.index ? 0x20 : 0x10;
						break;
					}

					if(is_active) state_ &= ~mask; else state_ |= mask;
				}

				uint8_t get_state() {
					return state_;
				}

			private:
				uint8_t state_ = 0xff;
		};
};

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public MachineTypes::TimedMachine,
	public MachineTypes::AudioProducer,
	public MachineTypes::ScanProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::JoystickMachine,
	public Configurable::Device,
	public MemoryMap,
	public ClockingHint::Observer,
	public Activity::Source {
	public:
		using Target = Analyser::Static::MSX::Target;

		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher):
			z80_(*this),
			vdp_(TI::TMS::TMS9918A),
			i8255_(i8255_port_handler_),
			ay_(GI::AY38910::Personality::AY38910, audio_queue_),
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
			tape_player_.set_clocking_hint_observer(this);

			// Set the AY to 50% of available volume, the toggle to 10% and leave 40% for an SCC.
			mixer_.set_relative_volumes({0.5f, 0.1f, 0.4f});

			// Install the proper TV standard and select an ideal BIOS name.
			const std::string machine_name = "MSX";
			std::vector<ROMMachine::ROM> required_roms = {
				{machine_name, "any MSX BIOS", "msx.rom", 32*1024, 0x94ee12f3}
			};

			bool is_ntsc = true;
			uint8_t character_generator = 1;	/* 0 = Japan, 1 = USA, etc, 2 = USSR */
			uint8_t date_format = 1;			/* 0 = Y/M/D, 1 = M/D/Y, 2 = D/M/Y */
			uint8_t keyboard = 1;				/* 0 = Japan, 1 = USA, 2 = France, 3 = UK, 4 = Germany, 5 = USSR, 6 = Spain */

			// TODO: CRCs below are incomplete, at best.
			switch(target.region) {
				case Target::Region::Japan:
					required_roms.emplace_back(machine_name, "a Japanese MSX BIOS", "msx-japanese.rom", 32*1024, 0xee229390);
					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);

					is_ntsc = true;
					character_generator = 0;
					date_format = 0;
				break;
				case Target::Region::USA:
					required_roms.emplace_back(machine_name, "an American MSX BIOS", "msx-american.rom", 32*1024, 0);
					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);

					is_ntsc = true;
					character_generator = 1;
					date_format = 1;
				break;
				case Target::Region::Europe:
					required_roms.emplace_back(machine_name, "a European MSX BIOS", "msx-european.rom", 32*1024, 0);
					vdp_->set_tv_standard(TI::TMS::TVStandard::PAL);

					is_ntsc = false;
					character_generator = 1;
					date_format = 2;
				break;
			}

			// Fetch the necessary ROMs; try the region-specific ROM first,
			// but failing that fall back on patching the main one.
			size_t disk_index = 0;
			if(target.has_disk_drive) {
				disk_index = required_roms.size();
				required_roms.emplace_back(machine_name, "the MSX-DOS ROM", "disk.rom", 16*1024, 0x721f61df);
			}
			const auto roms = rom_fetcher(required_roms);

			if((!roms[0] && !roms[1]) || (target.has_disk_drive && !roms[2])) {
				throw ROMMachine::Error::MissingROMs;
			}

			// Figure out which BIOS to use, either a specific one or the generic
			// one appropriately patched.
			if(roms[1]) {
				memory_slots_[0].source = std::move(*roms[1]);
				memory_slots_[0].source.resize(32768);
			} else {
				memory_slots_[0].source = std::move(*roms[0]);
				memory_slots_[0].source.resize(32768);

				memory_slots_[0].source[0x2b] = uint8_t(
					(is_ntsc ? 0x00 : 0x80) |
					(date_format << 4) |
					character_generator
				);
				memory_slots_[0].source[0x2c] = keyboard;
			}

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

			// Add a disk cartridge if any disks were supplied.
			if(target.has_disk_drive) {
				memory_slots_[2].set_handler(new DiskROM(memory_slots_[2].source));
				memory_slots_[2].source = std::move(*roms[disk_index]);
				memory_slots_[2].source.resize(16384);

				map(2, 0, 0x4000, 0x2000);
				unmap(2, 0x6000, 0x2000);
			}

			// Insert the media.
			insert_media(target.media);

			// Type whatever has been requested.
			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			vdp_->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return vdp_->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			vdp_->set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() final {
			return vdp_->get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		void run_for(const Cycles cycles) final {
			z80_.run_for(cycles);
		}

		float get_confidence() final {
			if(performed_unmapped_access_ || pc_zero_accesses_ > 1) return 0.0f;
			if(memory_slots_[1].handler) {
				return memory_slots_[1].handler->get_confidence();
			}
			return 0.5f;
		}

		std::string debug_type() final {
			if(memory_slots_[1].handler) {
				return "MSX:" + memory_slots_[1].handler->debug_type();
			}
			return "MSX";
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.cartridges.empty()) {
				const auto &segment = media.cartridges.front()->get_segments().front();
				memory_slots_[1].source = segment.data;
				map(1, 0, uint16_t(segment.start_address), std::min(segment.data.size(), 65536 - segment.start_address));

				auto msx_cartridge = dynamic_cast<Analyser::Static::MSX::Cartridge *>(media.cartridges.front().get());
				if(msx_cartridge) {
					switch(msx_cartridge->type) {
						default: break;
						case Analyser::Static::MSX::Cartridge::Konami:
							memory_slots_[1].set_handler(new Cartridge::KonamiROMSlotHandler(*this, 1));
						break;
						case Analyser::Static::MSX::Cartridge::KonamiWithSCC:
							memory_slots_[1].set_handler(new Cartridge::KonamiWithSCCROMSlotHandler(*this, 1, scc_));
						break;
						case Analyser::Static::MSX::Cartridge::ASCII8kb:
							memory_slots_[1].set_handler(new Cartridge::ASCII8kbROMSlotHandler(*this, 1));
						break;
						case Analyser::Static::MSX::Cartridge::ASCII16kb:
							memory_slots_[1].set_handler(new Cartridge::ASCII16kbROMSlotHandler(*this, 1));
						break;
					}
				}
			}

			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
			}

			if(!media.disks.empty()) {
				DiskROM *disk_rom = get_disk_rom();
				if(disk_rom) {
					size_t drive = 0;
					for(auto &disk : media.disks) {
						disk_rom->set_disk(disk, drive);
						drive++;
						if(drive == 2) break;
					}
				}
			}

			set_use_fast_tape();

			return true;
		}

		void type_string(const std::string &string) final {
			std::transform(
				string.begin(),
				string.end(),
				std::back_inserter(input_text_),
				[](unsigned char c) -> unsigned char { return (c == '\n') ? '\r' : c; }
			);
		}

		bool can_type(char c) final {
			// Make an effort to type the entire printable ASCII range.
			return c >= 32 && c < 127;
		}

		// MARK: MSX::MemoryMap
		void map(int slot, std::size_t source_address, uint16_t destination_address, std::size_t length) final {
			assert(!(destination_address & 8191));
			assert(!(length & 8191));
			assert(size_t(destination_address) + length <= 65536);

			for(std::size_t c = 0; c < (length >> 13); ++c) {
				if(memory_slots_[slot].wrapping_strategy == ROMSlotHandler::WrappingStrategy::Repeat) source_address %= memory_slots_[slot].source.size();
				memory_slots_[slot].read_pointers[(destination_address >> 13) + c] =
					(source_address < memory_slots_[slot].source.size()) ? &memory_slots_[slot].source[source_address] : unpopulated_;
				source_address += 8192;
			}

			page_memory(paged_memory_);
		}

		void unmap(int slot, uint16_t destination_address, std::size_t length) final {
			assert(!(destination_address & 8191));
			assert(!(length & 8191));
			assert(size_t(destination_address) + length <= 65536);

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
			set_use_fast_tape();
		}

		// MARK: Z80::BusHandler
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// Per the best information I currently have, the MSX inserts an extra cycle into each opcode read,
			// but otherwise runs without pause.
			const HalfCycles addition((cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) ? 2 : 0);
			const HalfCycles total_length = addition + cycle.length;
			vdp_ += total_length;
			time_since_ay_update_ += total_length;
			memory_slots_[0].cycles_since_update += total_length;
			memory_slots_[1].cycles_since_update += total_length;
			memory_slots_[2].cycles_since_update += total_length;
			memory_slots_[3].cycles_since_update += total_length;

			if(cycle.is_terminal()) {
				uint16_t address = cycle.address ? *cycle.address : 0x0000;
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
						if(use_fast_tape_) {
							if(address == 0x1a63) {
								// TAPION

								// Enable the tape motor.
								i8255_.write(0xab, 0x8);

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
									z80_.set_value_of_register(CPU::Z80::Register::A, uint16_t(next_byte));
									z80_.set_value_of_register(CPU::Z80::Register::Flags, 0);
								} else {
									z80_.set_value_of_register(CPU::Z80::Register::Flags, 1);
								}

								// RET.
								*cycle.value = 0xc9;
								break;
							}
						}

						if(!address) {
							pc_zero_accesses_++;
						}
						if(read_pointers_[address >> 13] == unpopulated_) {
							performed_unmapped_access_ = true;
						}
						pc_address_ = address;	// This is retained so as to be able to name the source of an access to cartridge handlers.
					case CPU::Z80::PartialMachineCycle::Read:
						if(read_pointers_[address >> 13]) {
							*cycle.value = read_pointers_[address >> 13][address & 8191];
						} else {
							int slot_hit = (paged_memory_ >> ((address >> 14) * 2)) & 3;
							memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.flush<HalfCycles>());
							*cycle.value = memory_slots_[slot_hit].handler->read(address);
						}
					break;

					case CPU::Z80::PartialMachineCycle::Write: {
						write_pointers_[address >> 13][address & 8191] = *cycle.value;

						int slot_hit = (paged_memory_ >> ((address >> 14) * 2)) & 3;
						if(memory_slots_[slot_hit].handler) {
							update_audio();
							memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.flush<HalfCycles>());
							memory_slots_[slot_hit].handler->write(address, *cycle.value, read_pointers_[pc_address_ >> 13] != memory_slots_[0].read_pointers[pc_address_ >> 13]);
						}
					} break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch(address & 0xff) {
							case 0x98:	case 0x99:
								*cycle.value = vdp_->read(address);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;

							case 0xa2:
								update_audio();
								ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BC1));
								*cycle.value = ay_.get_data_output();
								ay_.set_control_lines(GI::AY38910::ControlLines(0));
							break;

							case 0xa8:	case 0xa9:
							case 0xaa:	case 0xab:
								*cycle.value = i8255_.read(address);
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
								vdp_->write(address, *cycle.value);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;

							case 0xa0:	case 0xa1:
								update_audio();
								ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BDIR | GI::AY38910::BC2 | ((port == 0xa0) ? GI::AY38910::BC1 : 0)));
								ay_.set_data_input(*cycle.value);
								ay_.set_control_lines(GI::AY38910::ControlLines(0));
							break;

							case 0xa8:	case 0xa9:
							case 0xaa:	case 0xab:
								i8255_.write(address, *cycle.value);
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
							// The following are KEYBUF per the Red Book; its address and its definition as DEFS 40.
							const int buffer_start = 0xfbf0;
							const int buffer_size = 40;

							// Also from the Red Book: GETPNT is at F3FAH and PUTPNT is at F3F8H.
							int read_address = ram_[0xf3fa] | (ram_[0xf3fb] << 8);
							int write_address = ram_[0xf3f8] | (ram_[0xf3f9] << 8);

							// Write until either the string is exhausted or the write_pointer is immediately
							// behind the read pointer; temporarily map write_address and read_address into
							// buffer-relative values.
							std::size_t characters_written = 0;
							write_address -= buffer_start;
							read_address -= buffer_start;
							while(characters_written < input_text_.size()) {
								const int next_write_address = (write_address + 1) % buffer_size;
								if(next_write_address == read_address) break;
								ram_[write_address + buffer_start] = uint8_t(input_text_[characters_written]);
								++characters_written;
								write_address = next_write_address;
							}
							input_text_.erase(input_text_.begin(), input_text_.begin() + std::string::difference_type(characters_written));

							// Map the write address back into absolute terms and write it out again as PUTPNT.
							write_address += buffer_start;
							ram_[0xf3f8] = uint8_t(write_address);
							ram_[0xf3f9] = uint8_t(write_address >> 8);
						}
					break;

					default: break;
				}
			}

			if(!tape_player_is_sleeping_)
				tape_player_.run_for(int(cycle.length.as_integral()));

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= total_length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_interrupt_line(true, time_until_interrupt_);
				}
			}
			return addition;
		}

		void flush() {
			vdp_.flush();
			update_audio();
			audio_queue_.perform();
		}

		void set_keyboard_line(int line) {
			selected_key_line_ = line;
		}

		uint8_t read_keyboard() {
			return key_states_[selected_key_line_];
		}

		void clear_all_keys() final {
			std::memset(key_states_, 0xff, sizeof(key_states_));
		}

		void set_key_state(uint16_t key, bool is_pressed) final {
			int mask = 1 << (key & 7);
			int line = key >> 4;
			if(is_pressed) key_states_[line] &= ~mask; else key_states_[line] |= mask;
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			options->quickload = allow_fast_tape_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
			const auto options = dynamic_cast<Options *>(str.get());

			set_video_signal_configurable(options->output);
			allow_fast_tape_ = options->quickload;
			set_use_fast_tape();
		}

		// MARK: - Sleeper
		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) final {
			tape_player_is_sleeping_ = tape_player_.preferred_clocking() == ClockingHint::Preference::None;
			set_use_fast_tape();
		}

		// MARK: - Activity::Source
		void set_activity_observer(Activity::Observer *observer) final {
			DiskROM *disk_rom = get_disk_rom();
			if(disk_rom) {
				disk_rom->set_activity_observer(observer);
			}
			i8255_port_handler_.set_activity_observer(observer);
		}

		// MARK: - Joysticks
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return ay_port_handler_.get_joysticks();
		}

	private:
		DiskROM *get_disk_rom() {
			return dynamic_cast<DiskROM *>(memory_slots_[2].handler.get());
		}
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_ay_update_.divide_cycles(Cycles(2)));
		}

		class i8255PortHandler: public Intel::i8255::PortHandler {
			public:
				i8255PortHandler(ConcreteMachine &machine, Audio::Toggle &audio_toggle, Storage::Tape::BinaryTapePlayer &tape_player) :
					machine_(machine), audio_toggle_(audio_toggle), tape_player_(tape_player) {}

				void set_value(int port, uint8_t value) {
					switch(port) {
						case 0:	machine_.page_memory(value);	break;
						case 2: {
							// TODO:
							//	b6	caps lock LED
							//	b5	audio output

							//	b4: cassette motor relay
							tape_player_.set_motor_control(!(value & 0x10));
							if(activity_observer_) activity_observer_->set_led_status("Tape motor", !(value & 0x10));

							//	b7: keyboard click
							bool new_audio_level = !!(value & 0x80);
							if(audio_toggle_.get_output() != new_audio_level) {
								machine_.update_audio();
								audio_toggle_.set_output(new_audio_level);
							}

							// b0-b3: keyboard line
							machine_.set_keyboard_line(value & 0xf);
						} break;
						default: LOG("Unrecognised: MSX set 8255 output port " << port << " to value " << PADHEX(2) << value); break;
					}
				}

				uint8_t get_value(int port) {
					if(port == 1) {
						return machine_.read_keyboard();
					} else LOG("MSX attempted to read from 8255 port " << port);
					return 0xff;
				}

				void set_activity_observer(Activity::Observer *observer) {
					activity_observer_ = observer;
					if(activity_observer_) {
						activity_observer_->register_led("Tape motor");
						activity_observer_->set_led_status("Tape motor", tape_player_.get_motor_control());
					}
				}

			private:
				ConcreteMachine &machine_;
				Audio::Toggle &audio_toggle_;
				Storage::Tape::BinaryTapePlayer &tape_player_;
				Activity::Observer *activity_observer_ = nullptr;
		};

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		JustInTimeActor<TI::TMS::TMS9918> vdp_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910<false> ay_;
		Audio::Toggle audio_toggle_;
		Konami::SCC scc_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle, Konami::SCC> mixer_;
		Outputs::Speaker::LowpassSpeaker<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle, Konami::SCC>> speaker_;

		Storage::Tape::BinaryTapePlayer tape_player_;
		bool tape_player_is_sleeping_ = false;
		bool allow_fast_tape_ = false;
		bool use_fast_tape_ = false;
		void set_use_fast_tape() {
			use_fast_tape_ = !tape_player_is_sleeping_ && allow_fast_tape_ && tape_player_.has_tape() && !(paged_memory_&3);
		}

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

		HalfCycles time_since_ay_update_;
		HalfCycles time_until_interrupt_;

		uint8_t key_states_[16];
		int selected_key_line_ = 0;
		std::string input_text_;

		MSX::KeyboardMapper keyboard_mapper_;

		int pc_zero_accesses_ = 0;
		bool performed_unmapped_access_ = false;
		uint16_t pc_address_;
};

}

using namespace MSX;

Machine *Machine::MSX(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::MSX::Target;
	const Target *const msx_target = dynamic_cast<const Target *>(target);
	return new ConcreteMachine(*msx_target, rom_fetcher);
}

Machine::~Machine() {}
