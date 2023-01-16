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
#include "MemorySlotHandler.hpp"

#include "../../Analyser/Static/MSX/Cartridge.hpp"
#include "Cartridges/ASCII8kb.hpp"
#include "Cartridges/ASCII16kb.hpp"
#include "Cartridges/Konami.hpp"
#include "Cartridges/KonamiWithSCC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/1770/1770.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/9918/9918.hpp"
#include "../../Components/AudioToggle/AudioToggle.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/RP5C01/RP5C01.hpp"
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

using Target = Analyser::Static::MSX::Target;

template <Target::Model model>
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
	public ClockingHint::Observer,
	public Activity::Source,
	public MSX::MemorySlotChangeHandler {
	private:
		// Provide 512kb of memory for an MSX 2; 64kb for an MSX 1. 'Slightly' arbitrary.
		static constexpr size_t RAMSize = model == Target::Model::MSX2 ? 512 * 1024 : 64 * 1024;

		static constexpr int ClockRate = 3579545;

	public:
		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher):
			z80_(*this),
			i8255_(i8255_port_handler_),
			ay_(GI::AY38910::Personality::AY38910, audio_queue_),
			audio_toggle_(audio_queue_),
			scc_(audio_queue_),
			mixer_(ay_, audio_toggle_, scc_),
			speaker_(mixer_),
			tape_player_(3579545 * 2),
			i8255_port_handler_(*this, audio_toggle_, tape_player_),
			ay_port_handler_(tape_player_),
			memory_slots_{{*this}, {*this}, {*this}, {*this}},
			clock_(ClockRate) {
			set_clock_rate(ClockRate);
			clear_all_keys();

			ay_.set_port_handler(&ay_port_handler_);
			speaker_.set_input_rate(3579545.0f / 2.0f);
			tape_player_.set_clocking_hint_observer(this);

			// Set the AY to 50% of available volume, the toggle to 10% and leave 40% for an SCC.
			mixer_.set_relative_volumes({0.5f, 0.1f, 0.4f});

			// Install the proper TV standard and select an ideal BIOS name.
			const std::string machine_name = "MSX";
			constexpr ROM::Name bios_name = model == Target::Model::MSX1 ? ROM::Name::MSXGenericBIOS : ROM::Name::MSX2GenericBIOS;

			ROM::Request bios_request = ROM::Request(bios_name);
			if constexpr (model == Target::Model::MSX2) {
				bios_request = bios_request && ROM::Request(ROM::Name::MSX2Extension);
			}

			bool is_ntsc = true;
			uint8_t character_generator = 1;	/* 0 = Japan, 1 = USA, etc, 2 = USSR */
			uint8_t date_format = 1;			/* 0 = Y/M/D, 1 = M/D/Y, 2 = D/M/Y */
			uint8_t keyboard = 1;				/* 0 = Japan, 1 = USA, 2 = France, 3 = UK, 4 = Germany, 5 = USSR, 6 = Spain */
			ROM::Name regional_bios_name;

			switch(target.region) {
				default:
				case Target::Region::Japan:
					if constexpr (model == Target::Model::MSX1) {
						regional_bios_name = ROM::Name::MSXJapaneseBIOS;
					}
					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);

					is_ntsc = true;
					character_generator = 0;
					date_format = 0;
				break;
				case Target::Region::USA:
					if constexpr (model == Target::Model::MSX1) {
						regional_bios_name = ROM::Name::MSXAmericanBIOS;
					}
					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);

					is_ntsc = true;
					character_generator = 1;
					date_format = 1;
				break;
				case Target::Region::Europe:
					if constexpr (model == Target::Model::MSX1) {
						regional_bios_name = ROM::Name::MSXEuropeanBIOS;
					}
					vdp_->set_tv_standard(TI::TMS::TVStandard::PAL);

					is_ntsc = false;
					character_generator = 1;
					date_format = 2;
				break;
			}
			if constexpr (model == Target::Model::MSX1) {
				bios_request = bios_request || ROM::Request(regional_bios_name);
			}

			// Fetch the necessary ROMs; try the region-specific ROM first,
			// but failing that fall back on patching the main one.
			ROM::Request request;
			if(target.has_disk_drive) {
				request = ROM::Request(ROM::Name::MSXDOS) && bios_request;
			} else {
				request = bios_request;
			}

			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			// Figure out which BIOS to use, either a specific one or the generic
			// one appropriately patched.
			bool has_bios = false;
			if constexpr (model == Target::Model::MSX1) {
				const auto regional_bios = roms.find(regional_bios_name);
				if(regional_bios != roms.end()) {
					regional_bios->second.resize(32768);
					bios_slot().set_source(regional_bios->second);
					has_bios = true;
				}
			}
			if(!has_bios) {
				std::vector<uint8_t> &bios = roms.find(bios_name)->second;

				bios.resize(32768);

				// Modify the generic ROM to reflect the selected region, date format, etc.
				bios[0x2b] = uint8_t(
					(is_ntsc ? 0x00 : 0x80) |
					(date_format << 4) |
					character_generator
				);
				bios[0x2c] = keyboard;

				bios_slot().set_source(bios);
			}

			bios_slot().map(0, 0, 32768);

			ram_slot().resize_source(RAMSize);
			ram_slot().template map<MemorySlot::AccessType::ReadWrite>(0, 0, 65536);

			if constexpr (model == Target::Model::MSX2) {
				memory_slots_[3].supports_secondary_paging = true;

				const auto extension = roms.find(ROM::Name::MSX2Extension);
				extension->second.resize(32768);
				extension_rom_slot().set_source(extension->second);
				extension_rom_slot().map(0, 0, 32768);
			}

			// Add a disk cartridge if any disks were supplied.
			if(target.has_disk_drive) {
				disk_primary().handler = std::make_unique<DiskROM>(disk_slot());

				std::vector<uint8_t> &dos = roms.find(ROM::Name::MSXDOS)->second;
				dos.resize(16384);
				disk_slot().set_source(dos);

				disk_slot().map(0, 0x4000, 0x2000);
				disk_slot().unmap(0x6000, 0x2000);
			}

			// Insert the media.
			insert_media(target.media);

			// Type whatever has been requested.
			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			// Establish default paging.
			page_primary(0);
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

		Outputs::Display::DisplayType get_display_type() const final {
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
			if(cartridge_primary().handler) {
				return cartridge_primary().handler->get_confidence();
			}
			return 0.5f;
		}

		std::string debug_type() final {
			if(cartridge_primary().handler) {
				return "MSX:" + cartridge_primary().handler->debug_type();
			}
			return "MSX";
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.cartridges.empty()) {
				const auto &segment = media.cartridges.front()->get_segments().front();
				auto &slot = cartridge_slot();

				slot.set_source(segment.data);
				slot.map(0, uint16_t(segment.start_address), std::min(segment.data.size(), 65536 - segment.start_address));

				auto msx_cartridge = dynamic_cast<Analyser::Static::MSX::Cartridge *>(media.cartridges.front().get());
				if(msx_cartridge) {
					switch(msx_cartridge->type) {
						default: break;
						case Analyser::Static::MSX::Cartridge::Konami:
							cartridge_primary().handler = std::make_unique<Cartridge::KonamiROMSlotHandler>(static_cast<MSX::MemorySlot &>(slot));
						break;
						case Analyser::Static::MSX::Cartridge::KonamiWithSCC:
							cartridge_primary().handler = std::make_unique<Cartridge::KonamiWithSCCROMSlotHandler>(static_cast<MSX::MemorySlot &>(slot), scc_);
						break;
						case Analyser::Static::MSX::Cartridge::ASCII8kb:
							cartridge_primary().handler = std::make_unique<Cartridge::ASCII8kbROMSlotHandler>(static_cast<MSX::MemorySlot &>(slot));
						break;
						case Analyser::Static::MSX::Cartridge::ASCII16kb:
							cartridge_primary().handler = std::make_unique<Cartridge::ASCII16kbROMSlotHandler>(static_cast<MSX::MemorySlot &>(slot));
						break;
					}
				}
			}

			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
			}

			if(!media.disks.empty()) {
				DiskROM *const handler = disk_handler();
				if(handler) {
					size_t drive = 0;
					for(auto &disk : media.disks) {
						handler->set_disk(disk, drive);
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

		bool can_type(char c) const final {
			// Make an effort to type the entire printable ASCII range.
			return c >= 32 && c < 127;
		}

		// MARK: Memory paging.
		void page_primary(uint8_t value) {
			primary_slots_ = value;
			update_paging();
		}

		void did_page() final {
			update_paging();
		}

		void update_paging() {
			uint8_t primary = primary_slots_;

			// Update final slot; this direct pointer will be used for
			// secondary slot communication.
			final_slot_ = &memory_slots_[primary >> 6];

			for(int c = 0; c < 8; c += 2) {
				const PrimarySlot &slot = memory_slots_[primary & 3];
				primary >>= 2;

				read_pointers_[c] = slot.read_pointer(c);
				write_pointers_[c] = slot.write_pointer(c);
				read_pointers_[c+1] = slot.read_pointer(c+1);
				write_pointers_[c+1] = slot.write_pointer(c+1);
			}
			set_use_fast_tape();
		}

		// MARK: Z80::BusHandler
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// Per the best information I currently have, the MSX inserts an extra cycle into each opcode read,
			// but otherwise runs without pause.
			const HalfCycles addition((cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode) ? 2 : 0);
			const HalfCycles total_length = addition + cycle.length;
			if(vdp_ += total_length) {
				z80_.set_interrupt_line(vdp_->get_interrupt_line(), vdp_.last_sequence_point_overrun());
			}
			time_since_ay_update_ += total_length;
			memory_slots_[0].cycles_since_update += total_length;
			memory_slots_[1].cycles_since_update += total_length;
			memory_slots_[2].cycles_since_update += total_length;
			memory_slots_[3].cycles_since_update += total_length;

			if constexpr (model >= Target::Model::MSX2) {
				clock_.run_for(total_length);
			}

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
									ram()[0xfca4] = new_speed->minimum_start_bit_duration;
									ram()[0xfca5] = new_speed->low_high_disrimination_duration;
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
								tape_speed.minimum_start_bit_duration = ram()[0xfca4];
								tape_speed.low_high_disrimination_duration = ram()[0xfca5];

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

						// TODO: below relates to confidence measurements. Reinstate, somehow.
//						if(is_unpopulated_[address >> 13] == unpopulated_) {
//							performed_unmapped_access_ = true;
//						}

						pc_address_ = address;	// This is retained so as to be able to name the source of an access to cartridge handlers.
						[[fallthrough]];

					case CPU::Z80::PartialMachineCycle::Read:
						if(address == 0xffff && final_slot_->supports_secondary_paging) {
							*cycle.value = final_slot_->secondary_paging() ^ 0xff;
							break;
						}

						if(read_pointers_[address >> 13]) {
							*cycle.value = read_pointers_[address >> 13][address & 8191];
						} else {
							const int slot_hit = (primary_slots_ >> ((address >> 14) * 2)) & 3;
							memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.template flush<HalfCycles>());
							*cycle.value = memory_slots_[slot_hit].handler->read(address);
						}
					break;

					case CPU::Z80::PartialMachineCycle::Write: {
						if(address == 0xffff && final_slot_->supports_secondary_paging) {
							final_slot_->set_secondary_paging(*cycle.value);
							update_paging();
							break;
						}

						const int slot_hit = (primary_slots_ >> ((address >> 14) * 2)) & 3;
						if(memory_slots_[slot_hit].handler) {
							update_audio();
							memory_slots_[slot_hit].handler->run_for(memory_slots_[slot_hit].cycles_since_update.template flush<HalfCycles>());
							memory_slots_[slot_hit].handler->write(
								address,
								*cycle.value,
								read_pointers_[pc_address_ >> 13] != memory_slots_[0].read_pointer(pc_address_ >> 13));
						} else {
							write_pointers_[address >> 13][address & 8191] = *cycle.value;
						}
					} break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch(address & 0xff) {
							case 0x98:	case 0x99:
								*cycle.value = vdp_->read(address);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
							break;

							case 0xa2:
								update_audio();
								*cycle.value = GI::AY38910::Utility::read(ay_);
							break;

							case 0xa8:	case 0xa9:
							case 0xaa:	case 0xab:
								*cycle.value = i8255_.read(address);
							break;

							case 0xb5:
								if constexpr (model == Target::Model::MSX1) {
									break;
								}
								*cycle.value = clock_.read(next_clock_register_);
							break;

							default:
								printf("Unhandled read %02x\n", address & 0xff);
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
							break;

							case 0xa0:	case 0xa1:
								update_audio();
								GI::AY38910::Utility::write(ay_, port == 0xa1, *cycle.value);
							break;

							case 0xa8:	case 0xa9:
							case 0xaa:	case 0xab:
								i8255_.write(address, *cycle.value);
							break;

							case 0xb4:
								if constexpr (model == Target::Model::MSX1) {
									break;
								}
								next_clock_register_ = *cycle.value;
							break;
							case 0xb5:
								if constexpr (model == Target::Model::MSX1) {
									break;
								}
								clock_.write(next_clock_register_, *cycle.value);
							break;

							case 0xfc: case 0xfd: case 0xfe: case 0xff:
								// 1. Propagate to all handlers.
								// 2. Apply to RAM.
								printf("RAM banking %02x: %02x\n", port, *cycle.value);
							break;

							default:
								printf("Unhandled write %02x of %02x\n", address & 0xff, *cycle.value);
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
							int read_address = ram()[0xf3fa] | (ram()[0xf3fb] << 8);
							int write_address = ram()[0xf3f8] | (ram()[0xf3f9] << 8);

							// Write until either the string is exhausted or the write_pointer is immediately
							// behind the read pointer; temporarily map write_address and read_address into
							// buffer-relative values.
							std::size_t characters_written = 0;
							write_address -= buffer_start;
							read_address -= buffer_start;
							while(characters_written < input_text_.size()) {
								const int next_write_address = (write_address + 1) % buffer_size;
								if(next_write_address == read_address) break;
								ram()[write_address + buffer_start] = uint8_t(input_text_[characters_written]);
								++characters_written;
								write_address = next_write_address;
							}
							input_text_.erase(input_text_.begin(), input_text_.begin() + std::string::difference_type(characters_written));

							// Map the write address back into absolute terms and write it out again as PUTPNT.
							write_address += buffer_start;
							ram()[0xf3f8] = uint8_t(write_address);
							ram()[0xf3f9] = uint8_t(write_address >> 8);
						}
					break;

					default: break;
				}
			}

			if(!tape_player_is_sleeping_)
				tape_player_.run_for(int(cycle.length.as_integral()));

			return addition;
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
		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final {
			tape_player_is_sleeping_ = tape_player_.preferred_clocking() == ClockingHint::Preference::None;
			set_use_fast_tape();
		}

		// MARK: - Activity::Source
		void set_activity_observer(Activity::Observer *observer) final {
			DiskROM *handler = disk_handler();
			if(handler) {
				handler->set_activity_observer(observer);
			}
			i8255_port_handler_.set_activity_observer(observer);
		}

		// MARK: - Joysticks
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return ay_port_handler_.get_joysticks();
		}

	private:
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_ay_update_.divide_cycles(Cycles(2)));
		}

		class i8255PortHandler: public Intel::i8255::PortHandler {
			public:
				i8255PortHandler(ConcreteMachine &machine, Audio::Toggle &audio_toggle, Storage::Tape::BinaryTapePlayer &tape_player) :
					machine_(machine), audio_toggle_(audio_toggle), tape_player_(tape_player) {}

				void set_value(int port, uint8_t value) {
					switch(port) {
						case 0:	machine_.page_primary(value);	break;
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

		static constexpr TI::TMS::Personality vdp_model() {
			switch(model) {
				case Target::Model::MSX1:	return TI::TMS::Personality::TMS9918A;
				case Target::Model::MSX2:	return TI::TMS::Personality::V9938;
			}
		}

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		JustInTimeActor<TI::TMS::TMS9918<vdp_model()>> vdp_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		Concurrency::AsyncTaskQueue<false> audio_queue_;
		GI::AY38910::AY38910<false> ay_;
		Audio::Toggle audio_toggle_;
		Konami::SCC scc_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle, Konami::SCC> mixer_;
		Outputs::Speaker::PullLowpass<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle, Konami::SCC>> speaker_;

		Storage::Tape::BinaryTapePlayer tape_player_;
		bool tape_player_is_sleeping_ = false;
		bool allow_fast_tape_ = false;
		bool use_fast_tape_ = false;
		void set_use_fast_tape() {
			use_fast_tape_ =
				!tape_player_is_sleeping_ &&
				allow_fast_tape_ &&
				tape_player_.has_tape() &&
				!(primary_slots_ & 3) &&
				!(memory_slots_[0].secondary_paging() & 3);
		}

		i8255PortHandler i8255_port_handler_;
		AYPortHandler ay_port_handler_;

		/// The current primary and secondary slot selections; the former retains whatever was written
		/// last to the 8255 PPI via port A8 and the latter — if enabled — captures 0xffff on a per-slot basis.
		uint8_t primary_slots_ = 0;

		// Divides the current 64kb address space into 8kb chunks.
		// 8kb resolution is used by some cartride titles.
		const uint8_t *read_pointers_[8];
		uint8_t *write_pointers_[8];

		/// Optionally attaches non-default logic to any of the four things selectable
		/// via the primary slot register.
		struct PrimarySlot: public MSX::PrimarySlot {
			using MSX::PrimarySlot::PrimarySlot;
			HalfCycles cycles_since_update;

			/// Storage for a slot-specialised handler.
			std::unique_ptr<MemorySlotHandler> handler;
		};
		PrimarySlot memory_slots_[4];
		PrimarySlot *final_slot_ = nullptr;

		HalfCycles time_since_ay_update_;

		uint8_t key_states_[16];
		int selected_key_line_ = 0;
		std::string input_text_;

		MSX::KeyboardMapper keyboard_mapper_;

		int pc_zero_accesses_ = 0;
		bool performed_unmapped_access_ = false;
		uint16_t pc_address_;

		Ricoh::RP5C01::RP5C01 clock_;
		int next_clock_register_ = 0;

		//
		// Various helpers that dictate the slot arrangement used by this emulator.
		//
		MemorySlot &bios_slot() {
			return memory_slots_[0].subslot(0);
		}
		MemorySlot &ram_slot() {
			return memory_slots_[3].subslot(0);
		}
		MemorySlot &extension_rom_slot() {
			return memory_slots_[3].subslot(1);
		}

		MemorySlot &cartridge_slot() {
			return cartridge_primary().subslot(0);
		}
		MemorySlot &disk_slot() {
			return disk_primary().subslot(0);
		}

		PrimarySlot &cartridge_primary() {
			return memory_slots_[1];
		}
		PrimarySlot &disk_primary() {
			return memory_slots_[2];
		}

		uint8_t *ram() {
			return ram_slot().source().data();
		}
		DiskROM *disk_handler() {
			return dynamic_cast<DiskROM *>(disk_primary().handler.get());
		}};

}

using namespace MSX;

Machine *Machine::MSX(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const auto msx_target = dynamic_cast<const Target *>(target);
	switch(msx_target->model) {
		default:					return nullptr;
		case Target::Model::MSX1:	return new ConcreteMachine<Target::Model::MSX1>(*msx_target, rom_fetcher);
		case Target::Model::MSX2:	return new ConcreteMachine<Target::Model::MSX2>(*msx_target, rom_fetcher);
	}
}

Machine::~Machine() {}
