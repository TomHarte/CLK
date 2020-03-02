//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include "Keyboard.hpp"

#include "../../../Activity/Source.hpp"
#include "../../MediaTarget.hpp"
#include "../../CRTMachine.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../JoystickMachine.hpp"

#include "../../../Processors/6502/6502.hpp"
#include "../../../Components/6560/6560.hpp"
#include "../../../Components/6522/6522.hpp"

#include "../../../ClockReceiver/ForceInline.hpp"
#include "../../../Outputs/Log.hpp"

#include "../../../Storage/Tape/Parsers/Commodore.hpp"

#include "../SerialBus.hpp"
#include "../1540/C1540.hpp"

#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Storage/Disk/Disk.hpp"

#include "../../../Configurable/StandardOptions.hpp"

#include "../../../Analyser/Static/Commodore/Target.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace Commodore {
namespace Vic20 {

enum ROMSlot {
	Kernel = 0,
	BASIC,
	Characters,
	Drive
};

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(Configurable::DisplaySVideo | Configurable::DisplayCompositeColour | Configurable::QuickLoadTape)
	);
}

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20
};

/*!
	Models the user-port VIA, which is the Vic's connection point for controlling its tape recorder;
	sensing the presence or absence of a tape and controlling the tape motor; and reading the current
	state from its serial port. Most of the joystick input is also exposed here.
*/
class UserPortVIA: public MOS::MOS6522::IRQDelegatePortHandler {
	public:
		UserPortVIA() : port_a_(0xbf) {}

		/// Reports the current input to the 6522 port @c port.
		uint8_t get_port_input(MOS::MOS6522::Port port) {
			// Port A provides information about the presence or absence of a tape, and parts of
			// the joystick and serial port state, both of which have been statefully collected
			// into port_a_.
			if(!port) {
				return port_a_ | (tape_->has_tape() ? 0x00 : 0x40);
			}
			return 0xff;
		}

		/// Receives announcements of control line output change from the 6522.
		void set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
			// The CA2 output is used to control the tape motor.
			if(port == MOS::MOS6522::Port::A && line == MOS::MOS6522::Line::Two) {
				tape_->set_motor_control(!value);
			}
		}

		/// Receives announcements of changes in the serial bus connected to the serial port and propagates them into Port A.
		void set_serial_line_state(::Commodore::Serial::Line line, bool value) {
			switch(line) {
				default: break;
				case ::Commodore::Serial::Line::Data: port_a_ = (port_a_ & ~0x02) | (value ? 0x02 : 0x00);	break;
				case ::Commodore::Serial::Line::Clock: port_a_ = (port_a_ & ~0x01) | (value ? 0x01 : 0x00);	break;
			}
		}

		/// Allows the current joystick input to be set.
		void set_joystick_state(JoystickInput input, bool value) {
			if(input != JoystickInput::Right) {
				port_a_ = (port_a_ & ~input) | (value ? 0 : input);
			}
		}

		/// Receives announcements from the 6522 of user-port output, which might affect what's currently being presented onto the serial bus.
		void set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t mask) {
			// Line 7 of port A is inverted and output as serial ATN.
			if(!port) {
				std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
				if(serialPort) serialPort->set_output(::Commodore::Serial::Line::Attention, (::Commodore::Serial::LineLevel)!(value&0x80));
			}
		}

		/// Sets @serial_port as this VIA's connection to the serial bus.
		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serial_port) {
			serial_port_ = serial_port;
		}

		/// Sets @tape as the tape player connected to this VIA.
		void set_tape(std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape) {
			tape_ = tape;
		}

	private:
		uint8_t port_a_;
		std::weak_ptr<::Commodore::Serial::Port> serial_port_;
		std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape_;
};

/*!
	Models the keyboard VIA, which is used by the Vic for reading its keyboard, to output to its serial port,
	and for the small portion of joystick input not connected to the user-port VIA.
*/
class KeyboardVIA: public MOS::MOS6522::IRQDelegatePortHandler {
	public:
		KeyboardVIA() : port_b_(0xff) {
			clear_all_keys();
		}

		/// Sets whether @c key @c is_pressed.
		void set_key_state(uint16_t key, bool is_pressed) {
			if(is_pressed)
				columns_[key & 7] &= ~(key >> 3);
			else
				columns_[key & 7] |= (key >> 3);
		}

		/// Sets all keys as unpressed.
		void clear_all_keys() {
			memset(columns_, 0xff, sizeof(columns_));
		}

		/// Called by the 6522 to get input. Reads the keyboard on Port A, returns a small amount of joystick state on Port B.
		uint8_t get_port_input(MOS::MOS6522::Port port) {
			if(!port) {
				uint8_t result = 0xff;
				for(int c = 0; c < 8; c++) {
					if(!(activation_mask_&(1 << c)))
						result &= columns_[c];
				}
				return result;
			}

			return port_b_;
		}

		/// Called by the 6522 to set output. The value of Port B selects which part of the keyboard to read.
		void set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t mask) {
			if(port) activation_mask_ = (value & mask) | (~mask);
		}

		/// Called by the 6522 to set control line output. Which affects the serial port.
		void set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
			if(line == MOS::MOS6522::Line::Two) {
				std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
				if(serialPort) {
					// CB2 is inverted to become serial data; CA2 is inverted to become serial clock
					if(port == MOS::MOS6522::Port::A)
						serialPort->set_output(::Commodore::Serial::Line::Clock, (::Commodore::Serial::LineLevel)!value);
					else
						serialPort->set_output(::Commodore::Serial::Line::Data, (::Commodore::Serial::LineLevel)!value);
				}
			}
		}

		/// Sets whether the joystick input @c input is pressed.
		void set_joystick_state(JoystickInput input, bool value) {
			if(input == JoystickInput::Right) {
				port_b_ = (port_b_ & ~input) | (value ? 0 : input);
			}
		}

		/// Sets the serial port to which this VIA is connected.
		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
			serial_port_ = serialPort;
		}

	private:
		uint8_t port_b_;
		uint8_t columns_[8];
		uint8_t activation_mask_;
		std::weak_ptr<::Commodore::Serial::Port> serial_port_;
};

/*!
	Models the Vic's serial port, providing the receipticle for input.
*/
class SerialPort : public ::Commodore::Serial::Port {
	public:
		/// Receives an input change from the base serial port class, and communicates it to the user-port VIA.
		void set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level) {
			std::shared_ptr<UserPortVIA> userPortVIA = user_port_via_.lock();
			if(userPortVIA) userPortVIA->set_serial_line_state(line, static_cast<bool>(level));
		}

		/// Sets the user-port VIA with which this serial port communicates.
		void set_user_port_via(std::shared_ptr<UserPortVIA> userPortVIA) {
			user_port_via_ = userPortVIA;
		}

	private:
		std::weak_ptr<UserPortVIA> user_port_via_;
};

/*!
	Provides the bus over which the Vic 6560 fetches memory in a Vic-20.
*/
class Vic6560BusHandler {
	public:
		/// Performs a read on behalf of the 6560; in practice uses @c video_memory_map and @c colour_memory to find data.
		forceinline void perform_read(uint16_t address, uint8_t *pixel_data, uint8_t *colour_data) {
			*pixel_data = video_memory_map[address >> 10] ? video_memory_map[address >> 10][address & 0x3ff] : 0xff; // TODO
			*colour_data = colour_memory[address & 0x03ff];
		}

		// It is assumed that these pointers have been filled in by the machine.
		uint8_t *video_memory_map[16];	// Segments video memory into 1kb portions.
		uint8_t *colour_memory;			// Colour memory must be contiguous.
};

/*!
	Interfaces a joystick to the two VIAs.
*/
class Joystick: public Inputs::ConcreteJoystick {
	public:
		Joystick(UserPortVIA &user_port_via_port_handler, KeyboardVIA &keyboard_via_port_handler) :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),
				Input(Input::Fire)
			}),
			user_port_via_port_handler_(user_port_via_port_handler),
			keyboard_via_port_handler_(keyboard_via_port_handler) {}

		void did_set_input(const Input &digital_input, bool is_active) final {
			JoystickInput mapped_input;
			switch(digital_input.type) {
				default: return;
				case Input::Up:		mapped_input = Up;		break;
				case Input::Down:	mapped_input = Down;	break;
				case Input::Left:	mapped_input = Left;	break;
				case Input::Right:	mapped_input = Right;	break;
				case Input::Fire:	mapped_input = Fire;	break;
			}

			user_port_via_port_handler_.set_joystick_state(mapped_input, is_active);
			keyboard_via_port_handler_.set_joystick_state(mapped_input, is_active);
		}

	private:
		UserPortVIA &user_port_via_port_handler_;
		KeyboardVIA &keyboard_via_port_handler_;
};

class ConcreteMachine:
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public KeyboardMachine::MappedMachine,
	public JoystickMachine::Machine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Utility::TypeRecipient<CharacterMapper>,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Machine,
	public ClockingHint::Observer,
	public Activity::Source {
	public:
		ConcreteMachine(const Analyser::Static::Commodore::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
				m6502_(*this),
				mos6560_(mos6560_bus_handler_),
				user_port_via_port_handler_(new UserPortVIA),
				keyboard_via_port_handler_(new KeyboardVIA),
				serial_port_(new SerialPort),
				serial_bus_(new ::Commodore::Serial::Bus),
				user_port_via_(*user_port_via_port_handler_),
				keyboard_via_(*keyboard_via_port_handler_),
				tape_(new Storage::Tape::BinaryTapePlayer(1022727)) {
			// communicate the tape to the user-port VIA
			user_port_via_port_handler_->set_tape(tape_);

			// wire up the serial bus and serial port
			Commodore::Serial::AttachPortAndBus(serial_port_, serial_bus_);

			// wire up 6522s and serial port
			user_port_via_port_handler_->set_serial_port(serial_port_);
			keyboard_via_port_handler_->set_serial_port(serial_port_);
			serial_port_->set_user_port_via(user_port_via_port_handler_);

			// wire up the 6522s, tape and machine
			user_port_via_port_handler_->set_interrupt_delegate(this);
			keyboard_via_port_handler_->set_interrupt_delegate(this);
			tape_->set_delegate(this);
			tape_->set_clocking_hint_observer(this);

			// Install a joystick.
			joysticks_.emplace_back(new Joystick(*user_port_via_port_handler_, *keyboard_via_port_handler_));

			const std::string machine_name = "Vic20";
			std::vector<ROMMachine::ROM> rom_names = {
				{machine_name, "the VIC-20 BASIC ROM", "basic.bin", 8*1024, 0xdb4c43c1}
			};
			switch(target.region) {
				default:
					rom_names.emplace_back(machine_name, "the English-language VIC-20 character ROM", "characters-english.bin", 4*1024, 0x83e032a6);
					rom_names.emplace_back(machine_name, "the English-language PAL VIC-20 kernel ROM", "kernel-pal.bin", 8*1024, 0x4be07cb4);
				break;
				case Analyser::Static::Commodore::Target::Region::American:
					rom_names.emplace_back(machine_name, "the English-language VIC-20 character ROM", "characters-english.bin", 4*1024, 0x83e032a6);
					rom_names.emplace_back(machine_name, "the English-language NTSC VIC-20 kernel ROM", "kernel-ntsc.bin", 8*1024, 0xe5e7c174);
				break;
				case Analyser::Static::Commodore::Target::Region::Danish:
					rom_names.emplace_back(machine_name, "the Danish VIC-20 character ROM", "characters-danish.bin", 4*1024, 0x7fc11454);
					rom_names.emplace_back(machine_name, "the Danish VIC-20 kernel ROM", "kernel-danish.bin", 8*1024, 0x02adaf16);
				break;
				case Analyser::Static::Commodore::Target::Region::Japanese:
					rom_names.emplace_back(machine_name, "the Japanese VIC-20 character ROM", "characters-japanese.bin", 4*1024, 0xfcfd8a4b);
					rom_names.emplace_back(machine_name, "the Japanese VIC-20 kernel ROM", "kernel-japanese.bin", 8*1024, 0x336900d7);
				break;
				case Analyser::Static::Commodore::Target::Region::Swedish:
					rom_names.emplace_back(machine_name, "the Swedish VIC-20 character ROM", "characters-swedish.bin", 4*1024, 0xd808551d);
					rom_names.emplace_back(machine_name, "the Swedish VIC-20 kernel ROM", "kernel-swedish.bin", 8*1024, 0xb2a60662);
				break;
			}

			const auto roms = rom_fetcher(rom_names);

			for(const auto &rom: roms) {
				if(!rom) {
					throw ROMMachine::Error::MissingROMs;
				}
			}

			basic_rom_ = std::move(*roms[0]);
			character_rom_ = std::move(*roms[1]);
			kernel_rom_ = std::move(*roms[2]);

			// Characters ROMs should be 4kb.
			character_rom_.resize(4096);
			// Kernel ROMs and the BASIC ROM should be 8kb.
			kernel_rom_.resize(8192);

			if(target.has_c1540) {
				// construct the 1540
				c1540_ = std::make_unique<::Commodore::C1540::Machine>(Commodore::C1540::Personality::C1540, rom_fetcher);

				// attach it to the serial bus
				c1540_->set_serial_bus(serial_bus_);

				// give it a little warm up
				c1540_->run_for(Cycles(2000000));
			}

			// Determine PAL/NTSC
			if(target.region == Analyser::Static::Commodore::Target::Region::American || target.region == Analyser::Static::Commodore::Target::Region::Japanese) {
				// NTSC
				set_clock_rate(1022727);
				mos6560_.set_output_mode(MOS::MOS6560::OutputMode::NTSC);
			} else {
				// PAL
				set_clock_rate(1108404);
				mos6560_.set_output_mode(MOS::MOS6560::OutputMode::PAL);
			}

			mos6560_.set_high_frequency_cutoff(1600);	// There is a 1.6Khz low-pass filter in the Vic-20.
			mos6560_.set_clock_rate(get_clock_rate());

			// Initialise the memory maps as all pointing to nothing
			memset(processor_read_memory_map_, 0, sizeof(processor_read_memory_map_));
			memset(processor_write_memory_map_, 0, sizeof(processor_write_memory_map_));
			memset(mos6560_bus_handler_.video_memory_map, 0, sizeof(mos6560_bus_handler_.video_memory_map));

#define set_ram(baseaddr, length)	{ \
		write_to_map(processor_read_memory_map_, &ram_[baseaddr], baseaddr, length);	\
		write_to_map(processor_write_memory_map_, &ram_[baseaddr], baseaddr, length);	\
	}

			// Add 6502-visible RAM as requested.
			set_ram(0x0000, 0x0400);
			set_ram(0x1000, 0x1000);	// Built-in RAM.
			if(target.enabled_ram.bank0) set_ram(0x0400, 0x0c00);	// Bank 0:	0x0400 -> 0x1000.
			if(target.enabled_ram.bank1) set_ram(0x2000, 0x2000);	// Bank 1:	0x2000 -> 0x4000.
			if(target.enabled_ram.bank2) set_ram(0x4000, 0x2000);	// Bank 2:	0x4000 -> 0x6000.
			if(target.enabled_ram.bank3) set_ram(0x6000, 0x2000);	// Bank 3:	0x6000 -> 0x8000.
			if(target.enabled_ram.bank5) set_ram(0xa000, 0x2000);	// Bank 5:	0xa000 -> 0xc000.

#undef set_ram

			// all expansions also have colour RAM visible at 0x9400.
			write_to_map(processor_read_memory_map_, colour_ram_, 0x9400, sizeof(colour_ram_));
			write_to_map(processor_write_memory_map_, colour_ram_, 0x9400, sizeof(colour_ram_));

			// also push memory resources into the 6560 video memory map; the 6560 has only a
			// 14-bit address bus and the top bit is invested and used as bit 15 for the main
			// memory bus. It can access only internal memory, so the first 1kb, then the 4kb from 0x1000.
			struct Range {
				const std::size_t start, end;
				Range(std::size_t start, std::size_t end) : start(start), end(end) {}
			};
			const std::array<Range, 2> video_ranges = {{
				Range(0x0000, 0x0400),
				Range(0x1000, 0x2000),
			}};
			for(const auto &video_range : video_ranges) {
				for(auto addr = video_range.start; addr < video_range.end; addr += 0x400) {
					auto destination_address = (addr & 0x1fff) | (((addr & 0x8000) >> 2) ^ 0x2000);
					if(processor_read_memory_map_[addr >> 10]) {
						write_to_map(mos6560_bus_handler_.video_memory_map, &ram_[addr], static_cast<uint16_t>(destination_address), 0x400);
					}
				}
			}
			mos6560_bus_handler_.colour_memory = colour_ram_;

			// install the BASIC ROM
			write_to_map(processor_read_memory_map_, basic_rom_.data(), 0xc000, static_cast<uint16_t>(basic_rom_.size()));

			// install the system ROM
			write_to_map(processor_read_memory_map_, character_rom_.data(), 0x8000, static_cast<uint16_t>(character_rom_.size()));
			write_to_map(mos6560_bus_handler_.video_memory_map, character_rom_.data(), 0x0000, static_cast<uint16_t>(character_rom_.size()));
			write_to_map(processor_read_memory_map_, kernel_rom_.data(), 0xe000, static_cast<uint16_t>(kernel_rom_.size()));

			// The insert_media occurs last, so if there's a conflict between cartridges and RAM,
			// the cartridge wins.
			insert_media(target.media);
			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.tapes.empty()) {
				tape_->set_tape(media.tapes.front());
			}

			if(!media.disks.empty() && c1540_) {
				c1540_->set_disk(media.disks.front());
			}

			if(!media.cartridges.empty()) {
				rom_address_ = 0xa000;
				std::vector<uint8_t> rom_image = media.cartridges.front()->get_segments().front().data;
				rom_length_ = static_cast<uint16_t>(rom_image.size());

				rom_ = rom_image;
				rom_.resize(0x2000);
				write_to_map(processor_read_memory_map_, rom_.data(), rom_address_, rom_length_);
			}

			set_use_fast_tape();

			return !media.tapes.empty() || (!media.disks.empty() && c1540_ != nullptr) || !media.cartridges.empty();
		}

		void set_key_state(uint16_t key, bool is_pressed) final {
			if(key != KeyRestore)
				keyboard_via_port_handler_->set_key_state(key, is_pressed);
			else
				user_port_via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !is_pressed);
		}

		void clear_all_keys() final {
			keyboard_via_port_handler_->clear_all_keys();
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return joysticks_;
		}

		// to satisfy CPU::MOS6502::Processor
		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			// run the phase-1 part of this cycle, in which the VIC accesses memory
			cycles_since_mos6560_update_++;

			// run the phase-2 part of the cycle, which is whatever the 6502 said it should be
			if(isReadOperation(operation)) {
				uint8_t result = processor_read_memory_map_[address >> 10] ? processor_read_memory_map_[address >> 10][address & 0x3ff] : 0xff;
				if((address&0xfc00) == 0x9000) {
					if(!(address&0x100)) {
						update_video();
						result &= mos6560_.read(address);
					}
					if(address & 0x10) result &= user_port_via_.read(address);
					if(address & 0x20) result &= keyboard_via_.read(address);
				}
				*value = result;

				// Consider applying the fast tape hack.
				if(use_fast_tape_hack_ && operation == CPU::MOS6502::BusOperation::ReadOpcode) {
					if(address == 0xf7b2) {
						// Address 0xf7b2 contains a JSR to 0xf8c0 that will fill the tape buffer with the next header.
						// So cancel that via a double NOP and fill in the next header programmatically.
						Storage::Tape::Commodore::Parser parser;
						std::unique_ptr<Storage::Tape::Commodore::Header> header = parser.get_next_header(tape_->get_tape());

						const uint64_t tape_position = tape_->get_tape()->get_offset();
						if(header) {
							// serialise to wherever b2:b3 points
							const uint16_t tape_buffer_pointer = static_cast<uint16_t>(ram_[0xb2]) | static_cast<uint16_t>(ram_[0xb3] << 8);
							header->serialise(&ram_[tape_buffer_pointer], 0x8000 - tape_buffer_pointer);
							hold_tape_ = true;
							LOG("Vic-20: Found header");
						} else {
							// no header found, so pretend this hack never interceded
							tape_->get_tape()->set_offset(tape_position);
							hold_tape_ = false;
							LOG("Vic-20: Didn't find header");
						}

						// clear status and the verify flag
						ram_[0x90] = 0;
						ram_[0x93] = 0;

						*value = 0x0c;	// i.e. NOP abs, to swallow the entire JSR
					} else if(address == 0xf90b) {
						uint8_t x = static_cast<uint8_t>(m6502_.get_value_of_register(CPU::MOS6502::Register::X));
						if(x == 0xe) {
							Storage::Tape::Commodore::Parser parser;
							const uint64_t tape_position = tape_->get_tape()->get_offset();
							const std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(tape_->get_tape());
							if(data) {
								uint16_t start_address, end_address;
								start_address = static_cast<uint16_t>(ram_[0xc1] | (ram_[0xc2] << 8));
								end_address = static_cast<uint16_t>(ram_[0xae] | (ram_[0xaf] << 8));

								// perform a via-processor_write_memory_map_ memcpy
								uint8_t *data_ptr = data->data.data();
								std::size_t data_left = data->data.size();
								while(data_left && start_address != end_address) {
									uint8_t *page = processor_write_memory_map_[start_address >> 10];
									if(page) page[start_address & 0x3ff] = *data_ptr;
									data_ptr++;
									start_address++;
									data_left--;
								}

								// set tape status, carry and flag
								ram_[0x90] |= 0x40;
								uint8_t	flags = static_cast<uint8_t>(m6502_.get_value_of_register(CPU::MOS6502::Register::Flags));
								flags &= ~static_cast<uint8_t>((CPU::MOS6502::Flag::Carry | CPU::MOS6502::Flag::Interrupt));
								m6502_.set_value_of_register(CPU::MOS6502::Register::Flags, flags);

								// to ensure that execution proceeds to 0xfccf, pretend a NOP was here and
								// ensure that the PC leaps to 0xfccf
								m6502_.set_value_of_register(CPU::MOS6502::Register::ProgramCounter, 0xfccf);
								*value = 0xea;	// i.e. NOP implied
								hold_tape_ = true;
								LOG("Vic-20: Found data");
							} else {
								tape_->get_tape()->set_offset(tape_position);
								hold_tape_ = false;
								LOG("Vic-20: Didn't find data");
							}
						}
					}
				}
			} else {
				uint8_t *ram = processor_write_memory_map_[address >> 10];
				if(ram) {
					update_video();
					ram[address & 0x3ff] = *value;
				}
				// Anything between 0x9000 and 0x9400 is the IO area.
				if((address&0xfc00) == 0x9000) {
					// The VIC is selected by bit 8 = 0
					if(!(address&0x100)) {
						update_video();
						mos6560_.write(address, *value);
					}
					// The first VIA is selected by bit 4 = 1.
					if(address & 0x10) user_port_via_.write(address, *value);
					// The second VIA is selected by bit 5 = 1.
					if(address & 0x20) keyboard_via_.write(address, *value);
				}
			}

			user_port_via_.run_for(Cycles(1));
			keyboard_via_.run_for(Cycles(1));
			if(typer_ && address == 0xeb1e && operation == CPU::MOS6502::BusOperation::ReadOpcode) {
				if(!typer_->type_next_character()) {
					clear_all_keys();
					typer_.reset();
				}
			}
			if(!tape_is_sleeping_ && !hold_tape_) tape_->run_for(Cycles(1));
			if(c1540_) c1540_->run_for(Cycles(1));

			return Cycles(1);
		}

		void flush() {
			update_video();
			mos6560_.flush();
		}

		void run_for(const Cycles cycles) final {
			m6502_.run_for(cycles);
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			mos6560_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return mos6560_.get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			mos6560_.set_display_type(display_type);
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return mos6560_.get_speaker();
		}

		void mos6522_did_change_interrupt_status(void *mos6522) final {
			m6502_.set_nmi_line(user_port_via_.get_interrupt_line());
			m6502_.set_irq_line(keyboard_via_.get_interrupt_line());
		}

		void type_string(const std::string &string) final {
			Utility::TypeRecipient<CharacterMapper>::add_typer(string);
		}

		bool can_type(char c) final {
			return Utility::TypeRecipient<CharacterMapper>::can_type(c);
		}

		void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape) final {
			keyboard_via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !tape->get_input());
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() final {
			return Commodore::Vic20::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) final {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				allow_fast_tape_hack_ = quickload;
				set_use_fast_tape();
			}

			Configurable::Display display;
			if(Configurable::get_display(selections_by_option, display)) {
				set_video_signal_configurable(display);
			}
		}

		Configurable::SelectionSet get_accurate_selections() final {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			Configurable::append_display_selection(selection_set, Configurable::Display::CompositeColour);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() final {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			Configurable::append_display_selection(selection_set, Configurable::Display::SVideo);
			return selection_set;
		}

		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) final {
			tape_is_sleeping_ = clocking == ClockingHint::Preference::None;
			set_use_fast_tape();
		}

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) final {
			if(c1540_) c1540_->set_activity_observer(observer);
		}

	private:
		void update_video() {
			mos6560_.run_for(cycles_since_mos6560_update_.flush<Cycles>());
		}
		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

		std::vector<uint8_t>  character_rom_;
		std::vector<uint8_t>  basic_rom_;
		std::vector<uint8_t>  kernel_rom_;

		std::vector<uint8_t> rom_;
		uint16_t rom_address_, rom_length_;
		uint8_t ram_[0x10000];
		uint8_t colour_ram_[0x0400];

		uint8_t *processor_read_memory_map_[64];
		uint8_t *processor_write_memory_map_[64];
		void write_to_map(uint8_t **map, uint8_t *area, uint16_t address, uint16_t length) {
			address >>= 10;
			length >>= 10;
			while(length--) {
				map[address] = area;
				area += 0x400;
				address++;
			}
		}

		Commodore::Vic20::KeyboardMapper keyboard_mapper_;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		Cycles cycles_since_mos6560_update_;
		Vic6560BusHandler mos6560_bus_handler_;
		MOS::MOS6560::MOS6560<Vic6560BusHandler> mos6560_;
		std::shared_ptr<UserPortVIA> user_port_via_port_handler_;
		std::shared_ptr<KeyboardVIA> keyboard_via_port_handler_;
		std::shared_ptr<SerialPort> serial_port_;
		std::shared_ptr<::Commodore::Serial::Bus> serial_bus_;

		MOS::MOS6522::MOS6522<UserPortVIA> user_port_via_;
		MOS::MOS6522::MOS6522<KeyboardVIA> keyboard_via_;

		// Tape
		std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape_;
		bool use_fast_tape_hack_ = false;
		bool hold_tape_ = false;
		bool allow_fast_tape_hack_ = false;
		bool tape_is_sleeping_ = true;
		void set_use_fast_tape() {
			use_fast_tape_hack_ = !tape_is_sleeping_ && allow_fast_tape_hack_ && tape_->has_tape();
		}

		// Disk
		std::shared_ptr<::Commodore::C1540::Machine> c1540_;
};

}
}

using namespace Commodore::Vic20;

Machine *Machine::Vic20(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Commodore::Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return new Vic20::ConcreteMachine(*commodore_target, rom_fetcher);
}

Machine::~Machine() {}
