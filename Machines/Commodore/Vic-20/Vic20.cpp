//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include "Keyboard.hpp"

#include "../../../Processors/6502/6502.hpp"
#include "../../../Components/6560/6560.hpp"
#include "../../../Components/6522/6522.hpp"

#include "../../../ClockReceiver/ForceInline.hpp"

#include "../../../Storage/Tape/Parsers/Commodore.hpp"

#include "../SerialBus.hpp"
#include "../1540/C1540.hpp"

#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Storage/Disk/Disk.hpp"

#include "../../../Configurable/StandardOptions.hpp"

#include <algorithm>
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
	return Configurable::standard_options(Configurable::QuickLoadTape);
}

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20
};

enum ROM {
	CharactersDanish = 0,
	CharactersEnglish,
	CharactersJapanese,
	CharactersSwedish,
	KernelDanish,
	KernelJapanese,
	KernelNTSC,
	KernelPAL,
	KernelSwedish
};

/*!
	Models the user-port VIA, which is the Vic's connection point for controlling its tape recorder —
	sensing the presence or absence of a tape and controlling the tape motor — and reading the current
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
			if(userPortVIA) userPortVIA->set_serial_line_state(line, (bool)level);
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
class Vic6560: public MOS::MOS6560<Vic6560> {
	public:
		/// Performs a read on behalf of the 6560; in practice uses @c video_memory_map and @c colour_memory to find data.
		inline void perform_read(uint16_t address, uint8_t *pixel_data, uint8_t *colour_data) {
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
class Joystick: public Inputs::Joystick {
	public:
		Joystick(UserPortVIA &user_port_via_port_handler, KeyboardVIA &keyboard_via_port_handler) :
			user_port_via_port_handler_(user_port_via_port_handler),
			keyboard_via_port_handler_(keyboard_via_port_handler) {}

		void set_digital_input(DigitalInput digital_input, bool is_active) override {
			JoystickInput mapped_input;
			switch (digital_input) {
				default: return;
				case DigitalInput::Up: mapped_input = Up;		break;
				case DigitalInput::Down: mapped_input = Down;	break;
				case DigitalInput::Left: mapped_input = Left;	break;
				case DigitalInput::Right: mapped_input = Right;	break;
				case DigitalInput::Fire: mapped_input = Fire;	break;
			}

			user_port_via_port_handler_.set_joystick_state(mapped_input, is_active);
			keyboard_via_port_handler_.set_joystick_state(mapped_input, is_active);
		}

	private:
		UserPortVIA &user_port_via_port_handler_;
		KeyboardVIA &keyboard_via_port_handler_;
};

class ConcreteMachine:
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Machine {
	public:
		ConcreteMachine() :
				m6502_(*this),
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

			// install a joystick
			joysticks_.emplace_back(new Joystick(*user_port_via_port_handler_, *keyboard_via_port_handler_));
		}

		~ConcreteMachine() {
			delete[] rom_;
		}

		void set_rom(ROMSlot slot, const std::vector<uint8_t> &data) {
		}

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"Vic20",
				{
					"characters-danish.bin",
					"characters-english.bin",
					"characters-japanese.bin",
					"characters-swedish.bin",
					"kernel-danish.bin",
					"kernel-japanese.bin",
					"kernel-ntsc.bin",
					"kernel-pal.bin",
					"kernel-swedish.bin",
					"basic.bin"
				});

			for(std::size_t index = 0; index < roms.size(); ++index) {
				auto &data = roms[index];
				if(!data) return false;
				if(index < 9) roms_[index] = std::move(*data); else basic_rom_ = std::move(*data);
			}

			// Characters ROMs should be 4kb.
			for(std::size_t index = 0; index < 4; ++index) roms_[index].resize(4096);
			// Kernel ROMs and the BASIC ROM should be 8kb.
			for(std::size_t index = 4; index < roms.size(); ++index) roms_[index].resize(8192);

			return true;
		}

		void configure_as_target(const StaticAnalyser::Target &target) override final {
			if(target.loadingCommand.length()) {
				set_typer_for_string(target.loadingCommand.c_str());
			}

			switch(target.vic20.memory_model) {
				case StaticAnalyser::Vic20MemoryModel::Unexpanded:
					set_memory_size(Default);
				break;
				case StaticAnalyser::Vic20MemoryModel::EightKB:
					set_memory_size(ThreeKB);
				break;
				case StaticAnalyser::Vic20MemoryModel::ThirtyTwoKB:
					set_memory_size(ThirtyTwoKB);
				break;
			}

			if(target.media.disks.size()) {
				// construct the 1540
				c1540_.reset(new ::Commodore::C1540::Machine(Commodore::C1540::Machine::C1540));

				// attach it to the serial bus
				c1540_->set_serial_bus(serial_bus_);

				// give it a means to obtain its ROM
				c1540_->set_rom_fetcher(rom_fetcher_);
			}

			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override final {
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

				rom_ = new uint8_t[0x2000];
				std::memcpy(rom_, rom_image.data(), rom_image.size());
				write_to_map(processor_read_memory_map_, rom_, rom_address_, 0x2000);
			}

			return !media.tapes.empty() || (!media.disks.empty() && c1540_ != nullptr) || !media.cartridges.empty();
		}

		void set_key_state(uint16_t key, bool is_pressed) override final {
			if(key != KeyRestore)
				keyboard_via_port_handler_->set_key_state(key, is_pressed);
			else
				user_port_via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !is_pressed);
		}

		void clear_all_keys() override final {
			keyboard_via_port_handler_->clear_all_keys();
		}

		std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}

		void set_memory_size(MemorySize size) override final {
			memory_size_ = size;
			needs_configuration_ = true;
		}

		void set_region(Region region) override final {
			region_ = region;
			needs_configuration_ = true;
		}

		void set_ntsc_6560() {
			set_clock_rate(1022727);
			if(mos6560_) {
				mos6560_->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::NTSC);
				mos6560_->set_clock_rate(1022727);
			}
		}

		void set_pal_6560() {
			set_clock_rate(1108404);
			if(mos6560_) {
				mos6560_->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::PAL);
				mos6560_->set_clock_rate(1108404);
			}
		}

		void configure_memory() {
			// Determine PAL/NTSC
			if(region_ == American || region_ == Japanese) {
				// NTSC
				set_ntsc_6560();
			} else {
				// PAL
				set_pal_6560();
			}

			memset(processor_read_memory_map_, 0, sizeof(processor_read_memory_map_));
			memset(processor_write_memory_map_, 0, sizeof(processor_write_memory_map_));
			memset(mos6560_->video_memory_map, 0, sizeof(mos6560_->video_memory_map));

			switch(memory_size_) {
				default: break;
				case ThreeKB:
					write_to_map(processor_read_memory_map_, expansion_ram_, 0x0000, 0x1000);
					write_to_map(processor_write_memory_map_, expansion_ram_, 0x0000, 0x1000);
				break;
				case ThirtyTwoKB:
					write_to_map(processor_read_memory_map_, expansion_ram_, 0x0000, 0x8000);
					write_to_map(processor_write_memory_map_, expansion_ram_, 0x0000, 0x8000);
				break;
			}

			// install the system ROMs and VIC-visible memory
			write_to_map(processor_read_memory_map_, user_basic_memory_, 0x0000, sizeof(user_basic_memory_));
			write_to_map(processor_read_memory_map_, screen_memory_, 0x1000, sizeof(screen_memory_));
			write_to_map(processor_read_memory_map_, colour_memory_, 0x9400, sizeof(colour_memory_));

			write_to_map(processor_write_memory_map_, user_basic_memory_, 0x0000, sizeof(user_basic_memory_));
			write_to_map(processor_write_memory_map_, screen_memory_, 0x1000, sizeof(screen_memory_));
			write_to_map(processor_write_memory_map_, colour_memory_, 0x9400, sizeof(colour_memory_));

			write_to_map(mos6560_->video_memory_map, user_basic_memory_, 0x2000, sizeof(user_basic_memory_));
			write_to_map(mos6560_->video_memory_map, screen_memory_, 0x3000, sizeof(screen_memory_));
			mos6560_->colour_memory = colour_memory_;

			write_to_map(processor_read_memory_map_, basic_rom_.data(), 0xc000, static_cast<uint16_t>(basic_rom_.size()));

			ROM character_rom;
			ROM kernel_rom;
			switch(region_) {
				default:
					character_rom = CharactersEnglish;
					kernel_rom = KernelPAL;
				break;
				case American:
					character_rom = CharactersEnglish;
					kernel_rom = KernelNTSC;
				break;
				case Danish:
					character_rom = CharactersDanish;
					kernel_rom = KernelDanish;
				break;
				case Japanese:
					character_rom = CharactersJapanese;
					kernel_rom = KernelJapanese;
				break;
				case Swedish:
					character_rom = CharactersSwedish;
					kernel_rom = KernelSwedish;
				break;
			}

			write_to_map(processor_read_memory_map_, roms_[character_rom].data(), 0x8000, static_cast<uint16_t>(roms_[character_rom].size()));
			write_to_map(mos6560_->video_memory_map, roms_[character_rom].data(), 0x0000, static_cast<uint16_t>(roms_[character_rom].size()));
			write_to_map(processor_read_memory_map_, roms_[kernel_rom].data(), 0xe000, static_cast<uint16_t>(roms_[kernel_rom].size()));

			// install the inserted ROM if there is one
			if(rom_) {
				write_to_map(processor_read_memory_map_, rom_, rom_address_, rom_length_);
			}
		}

		void set_use_fast_tape_hack(bool activate) {
			use_fast_tape_hack_ = activate;
		}

		// to satisfy CPU::MOS6502::Processor
		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			// run the phase-1 part of this cycle, in which the VIC accesses memory
			if(!is_running_at_zero_cost_) mos6560_->run_for(Cycles(1));

			// run the phase-2 part of the cycle, which is whatever the 6502 said it should be
			if(isReadOperation(operation)) {
				uint8_t result = processor_read_memory_map_[address >> 10] ? processor_read_memory_map_[address >> 10][address & 0x3ff] : 0xff;
				if((address&0xfc00) == 0x9000) {
					if((address&0xff00) == 0x9000)	result &= mos6560_->get_register(address);
					if((address&0xfc10) == 0x9010)	result &= user_port_via_.get_register(address);
					if((address&0xfc20) == 0x9020)	result &= keyboard_via_.get_register(address);
				}
				*value = result;

				// This combined with the stuff below constitutes the fast tape hack. Performed here: if the
				// PC hits the start of the loop that just waits for an interesting tape interrupt to have
				// occurred then skip both 6522s and the tape ahead to the next interrupt without any further
				// CPU or 6560 costs.
				if(use_fast_tape_hack_ && tape_->has_tape() && operation == CPU::MOS6502::BusOperation::ReadOpcode) {
					if(address == 0xf7b2) {
						// Address 0xf7b2 contains a JSR to 0xf8c0 that will fill the tape buffer with the next header.
						// So cancel that via a double NOP and fill in the next header programmatically.
						Storage::Tape::Commodore::Parser parser;
						std::unique_ptr<Storage::Tape::Commodore::Header> header = parser.get_next_header(tape_->get_tape());

						// serialise to wherever b2:b3 points
						uint16_t tape_buffer_pointer = static_cast<uint16_t>(user_basic_memory_[0xb2]) | static_cast<uint16_t>(user_basic_memory_[0xb3] << 8);
						if(header) {
							header->serialise(&user_basic_memory_[tape_buffer_pointer], 0x8000 - tape_buffer_pointer);
						} else {
							// no header found, so store end-of-tape
							user_basic_memory_[tape_buffer_pointer] = 0x05;	// i.e. end of tape
						}

						// clear status and the verify flag
						user_basic_memory_[0x90] = 0;
						user_basic_memory_[0x93] = 0;

						*value = 0x0c;	// i.e. NOP abs
					} else if(address == 0xf90b) {
						uint8_t x = static_cast<uint8_t>(m6502_.get_value_of_register(CPU::MOS6502::Register::X));
						if(x == 0xe) {
							Storage::Tape::Commodore::Parser parser;
							std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(tape_->get_tape());
							uint16_t start_address, end_address;
							start_address = static_cast<uint16_t>(user_basic_memory_[0xc1] | (user_basic_memory_[0xc2] << 8));
							end_address = static_cast<uint16_t>(user_basic_memory_[0xae] | (user_basic_memory_[0xaf] << 8));

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
							user_basic_memory_[0x90] |= 0x40;
							uint8_t	flags = static_cast<uint8_t>(m6502_.get_value_of_register(CPU::MOS6502::Register::Flags));
							flags &= ~static_cast<uint8_t>((CPU::MOS6502::Flag::Carry | CPU::MOS6502::Flag::Interrupt));
							m6502_.set_value_of_register(CPU::MOS6502::Register::Flags, flags);

							// to ensure that execution proceeds to 0xfccf, pretend a NOP was here and
							// ensure that the PC leaps to 0xfccf
							m6502_.set_value_of_register(CPU::MOS6502::Register::ProgramCounter, 0xfccf);
							*value = 0xea;	// i.e. NOP implied
						}
					}
				}
			} else {
				uint8_t *ram = processor_write_memory_map_[address >> 10];
				if(ram) ram[address & 0x3ff] = *value;
				if((address&0xfc00) == 0x9000) {
					if((address&0xff00) == 0x9000)	mos6560_->set_register(address, *value);
					if((address&0xfc10) == 0x9010)	user_port_via_.set_register(address, *value);
					if((address&0xfc20) == 0x9020)	keyboard_via_.set_register(address, *value);
				}
			}

			user_port_via_.run_for(Cycles(1));
			keyboard_via_.run_for(Cycles(1));
			if(typer_ && operation == CPU::MOS6502::BusOperation::ReadOpcode && address == 0xEB1E) {
				if(!typer_->type_next_character()) {
					clear_all_keys();
					typer_.reset();
				}
			}
			tape_->run_for(Cycles(1));
			if(c1540_) c1540_->run_for(Cycles(1));

			return Cycles(1);
		}

		forceinline void flush() {
			mos6560_->flush();
		}

		void run_for(const Cycles cycles) override final {
			if(needs_configuration_) {
				needs_configuration_ = false;
				configure_memory();
			}
			m6502_.run_for(cycles);
		}

		void setup_output(float aspect_ratio) override final {
			mos6560_.reset(new Vic6560());
			mos6560_->set_high_frequency_cutoff(1600);	// There is a 1.6Khz low-pass filter in the Vic-20.
			// Make a guess: PAL. Without setting a clock rate the 6560 isn't fully set up so contractually something must be set.
			set_pal_6560();
		}

		void close_output() override final {
			mos6560_ = nullptr;
		}

		Outputs::CRT::CRT *get_crt() override final {
			return mos6560_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override final {
			return mos6560_->get_speaker();
		}

		void mos6522_did_change_interrupt_status(void *mos6522) override final {
			m6502_.set_nmi_line(user_port_via_.get_interrupt_line());
			m6502_.set_irq_line(keyboard_via_.get_interrupt_line());
		}

		void set_typer_for_string(const char *string) override final {
			std::unique_ptr<CharacterMapper> mapper(new CharacterMapper());
			Utility::TypeRecipient::set_typer_for_string(string, std::move(mapper));
		}

		void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape) override final {
			keyboard_via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !tape->get_input());
		}

		KeyboardMapper &get_keyboard_mapper() override {
			return keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return Commodore::Vic20::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				set_use_fast_tape_hack(quickload);
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			return selection_set;
		}

	private:
		CPU::MOS6502::Processor<ConcreteMachine, false> m6502_;

		std::vector<uint8_t>  roms_[9];

		std::vector<uint8_t>  character_rom_;
		std::vector<uint8_t>  basic_rom_;
		std::vector<uint8_t>  kernel_rom_;
		uint8_t expansion_ram_[0x8000];

		uint8_t *rom_ = nullptr;
		uint16_t rom_address_, rom_length_;

		uint8_t user_basic_memory_[0x0400];
		uint8_t screen_memory_[0x1000];
		uint8_t colour_memory_[0x0400];

		std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> rom_fetcher_;

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

		Region region_ = European;
		MemorySize memory_size_ = MemorySize::Default;
		bool needs_configuration_ = true;

		Commodore::Vic20::KeyboardMapper keyboard_mapper_;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		std::unique_ptr<Vic6560> mos6560_;
		std::shared_ptr<UserPortVIA> user_port_via_port_handler_;
		std::shared_ptr<KeyboardVIA> keyboard_via_port_handler_;
		std::shared_ptr<SerialPort> serial_port_;
		std::shared_ptr<::Commodore::Serial::Bus> serial_bus_;

		MOS::MOS6522::MOS6522<UserPortVIA> user_port_via_;
		MOS::MOS6522::MOS6522<KeyboardVIA> keyboard_via_;

		// Tape
		std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape_;
		bool use_fast_tape_hack_;
		bool is_running_at_zero_cost_ = false;

		// Disk
		std::shared_ptr<::Commodore::C1540::Machine> c1540_;
};

}
}

using namespace Commodore::Vic20;

Machine *Machine::Vic20() {
	return new Vic20::ConcreteMachine;
}

Machine::~Machine() {}
