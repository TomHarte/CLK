//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include "Keyboard.hpp"

#include "Activity/Source.hpp"
#include "Machines/MachineTypes.hpp"

#include "Processors/6502Mk2/6502Mk2.hpp"
#include "Components/6560/6560.hpp"
#include "Components/6522/6522.hpp"

#include "ClockReceiver/ForceInline.hpp"
#include "Outputs/Log.hpp"

#include "Storage/Tape/Parsers/Commodore.hpp"

#include "Machines/Commodore/SerialBus.hpp"
#include "Machines/Commodore/1540/C1540.hpp"

#include "Storage/Tape/Tape.hpp"
#include "Storage/Disk/Disk.hpp"

#include "Configurable/StandardOptions.hpp"

#include "Analyser/Dynamic/ConfidenceCounter.hpp"
#include "Analyser/Static/Commodore/Target.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {
using Logger = Log::Logger<Log::Source::Vic20>;
}

namespace Commodore::Vic20 {

enum ROMSlot {
	Kernel = 0,
	BASIC,
	Characters,
	Drive,
};

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20,
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
	template <MOS::MOS6522::Port port> uint8_t get_port_input() const {
		// Port A provides information about the presence or absence of a tape, and parts of
		// the joystick and serial port state, both of which have been statefully collected
		// into port_a_.
		if constexpr (port == MOS::MOS6522::Port::A) {
			return port_a_ | (tape_->has_tape() ? 0x00 : 0x40);
		}
		return 0xff;
	}

	/// Receives announcements of control line output change from the 6522.
	template <MOS::MOS6522::Port port, MOS::MOS6522::Line line>
	void set_control_line_output(const bool value) {
		// CA2: control the tape motor.
		if constexpr (port == MOS::MOS6522::Port::A && line == MOS::MOS6522::Line::Two) {
			tape_->set_motor_control(!value);
		}
	}

	/// Receives announcements of changes in the serial bus connected to the serial port and propagates them into Port A.
	void set_serial_line_state(const Commodore::Serial::Line line, const bool value) {
		const auto set = [&](const uint8_t bit) {
			port_a_ = (port_a_ & ~bit) | (value ? bit : 0x00);
		};
		switch(line) {
			default: break;
			case ::Commodore::Serial::Line::Data: 	set(0x02);	break;
			case ::Commodore::Serial::Line::Clock: 	set(0x01);	break;
		}
	}

	/// Allows the current joystick input to be set.
	void set_joystick_state(const JoystickInput input, const bool value) {
		if(input != JoystickInput::Right) {
			port_a_ = (port_a_ & ~input) | (value ? 0 : input);
		}
	}

	/// Receives announcements from the 6522 of user-port output, which might affect what's currently being presented onto the serial bus.
	template <MOS::MOS6522::Port port> void set_port_output(const uint8_t value, uint8_t) {
		// Line 7 of port A is inverted and output as serial ATN.
		if(!port) {
			serial_port_->set_output(Serial::Line::Attention, Serial::LineLevel(!(value&0x80)));
		}
	}

	/// Sets @serial_port as this VIA's connection to the serial bus.
	void set_serial_port(Serial::Port &serial_port) {
		serial_port_ = &serial_port;
	}

	/// Sets @tape as the tape player connected to this VIA.
	void set_tape(std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape) {
		tape_ = tape;
	}

private:
	uint8_t port_a_;
	Serial::Port *serial_port_ = nullptr;
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
	void set_key_state(const uint16_t key, const bool is_pressed) {
		if(is_pressed)
			columns_[key & 7] &= ~(key >> 3);
		else
			columns_[key & 7] |= (key >> 3);
	}

	/// Sets all keys as unpressed.
	void clear_all_keys() {
		std::fill(std::begin(columns_), std::end(columns_), 0xff);
	}

	/// Called by the 6522 to get input. Reads the keyboard on Port A, returns a small amount of joystick state on Port B.
	template <MOS::MOS6522::Port port> uint8_t get_port_input() const {
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
	template <MOS::MOS6522::Port port> void set_port_output(const uint8_t value, const uint8_t mask) {
		if(port) activation_mask_ = (value & mask) | (~mask);
	}

	/// Called by the 6522 to set control line output. Which affects the serial port.
	template <MOS::MOS6522::Port port, MOS::MOS6522::Line line> void set_control_line_output(const bool value) {
		if(line == MOS::MOS6522::Line::Two) {
			// CB2 is inverted to become serial data; CA2 is inverted to become serial clock
			if(port == MOS::MOS6522::Port::A)
				serial_port_->set_output(Serial::Line::Clock, Serial::LineLevel(!value));
			else
				serial_port_->set_output(Serial::Line::Data, Serial::LineLevel(!value));
		}
	}

	/// Sets whether the joystick input @c input is pressed.
	void set_joystick_state(const JoystickInput input, const bool value) {
		if(input == JoystickInput::Right) {
			port_b_ = (port_b_ & ~input) | (value ? 0 : input);
		}
	}

	/// Sets the serial port to which this VIA is connected.
	void set_serial_port(Serial::Port &port) {
		serial_port_ = &port;
	}

private:
	uint8_t port_b_;
	uint8_t columns_[8];
	uint8_t activation_mask_;
	Serial::Port *serial_port_ = nullptr;
};

/*!
	Models the Vic's serial port, providing the receipticle for input.
*/
class SerialPort : public ::Commodore::Serial::Port {
public:
	/// Receives an input change from the base serial port class, and communicates it to the user-port VIA.
	void set_input(const ::Commodore::Serial::Line line, const ::Commodore::Serial::LineLevel level) {
		if(user_port_via_) user_port_via_->set_serial_line_state(line, bool(level));
	}

	/// Sets the user-port VIA with which this serial port communicates.
	void set_user_port_via(UserPortVIA &via) {
		user_port_via_ = &via;
	}

private:
	UserPortVIA *user_port_via_ = nullptr;
};

/*!
	Provides the bus over which the Vic 6560 fetches memory in a Vic-20.
*/
struct Vic6560BusHandler {
	/// Performs a read on behalf of the 6560; in practice uses @c video_memory_map and @c colour_memory to find data.
	forceinline void perform_read(const uint16_t address, uint8_t *const pixel_data, uint8_t *const colour_data) const {
		*pixel_data = video_memory_map[address >> 10] ? video_memory_map[address >> 10][address & 0x3ff] : 0xff; // TODO
		*colour_data = colour_memory[address & 0x03ff];
	}

	// It is assumed that these pointers have been filled in by the machine.
	const uint8_t *video_memory_map[16]{};	// Segments video memory into 1kb portions.
	const uint8_t *colour_memory{};			// Colour memory must be contiguous.
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

	void did_set_input(const Input &digital_input, const bool is_active) final {
		if(const auto mapped_input = [&]() -> std::optional<JoystickInput> {
				switch(digital_input.type) {
					default:			return std::nullopt;
					case Input::Up:		return Up;
					case Input::Down:	return Down;
					case Input::Left:	return Left;
					case Input::Right:	return Right;
					case Input::Fire:	return Fire;
				}
			}(); mapped_input.has_value()
		) {
			user_port_via_port_handler_.set_joystick_state(*mapped_input, is_active);
			keyboard_via_port_handler_.set_joystick_state(*mapped_input, is_active);
		}
	}

private:
	UserPortVIA &user_port_via_port_handler_;
	KeyboardVIA &keyboard_via_port_handler_;
};

class ConcreteMachine:
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::JoystickMachine,
	public Configurable::Device,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Utility::TypeRecipient<CharacterMapper>,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Machine,
	public ClockingHint::Observer,
	public Activity::Source {
public:
	ConcreteMachine(const Analyser::Static::Commodore::Vic20Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m6502_(*this),
			mos6560_(mos6560_bus_handler_),
			user_port_via_(user_port_via_port_handler_),
			keyboard_via_(keyboard_via_port_handler_),
			tape_(new Storage::Tape::BinaryTapePlayer(1022727)) {
		// Connect tape and user-port VIA.
		user_port_via_port_handler_.set_tape(tape_);

		// Connect serial bus and serial port.
		Commodore::Serial::attach(serial_port_, serial_bus_);

		// Connect 6522s and serial port.
		user_port_via_port_handler_.set_serial_port(serial_port_);
		keyboard_via_port_handler_.set_serial_port(serial_port_);
		serial_port_.set_user_port_via(user_port_via_port_handler_);

		// Connect 6522s, tape and machine.
		user_port_via_port_handler_.set_interrupt_delegate(this);
		keyboard_via_port_handler_.set_interrupt_delegate(this);
		tape_->set_delegate(this);
		tape_->set_clocking_hint_observer(this);

		// Install a joystick.
		joysticks_.emplace_back(new Joystick(user_port_via_port_handler_, keyboard_via_port_handler_));

		// Obtain and distribute ROMs.
		ROM::Request request(ROM::Name::Vic20BASIC);
		ROM::Name kernel, character;
		using Region = Analyser::Static::Commodore::Vic20Target::Region;
		switch(target.region) {
			default:
				character = ROM::Name::Vic20EnglishCharacters;
				kernel = ROM::Name::Vic20EnglishPALKernel;
			break;
			case Region::American:
				character = ROM::Name::Vic20EnglishCharacters;
				kernel = ROM::Name::Vic20EnglishNTSCKernel;
			break;
			case Region::Danish:
				character = ROM::Name::Vic20DanishCharacters;
				kernel = ROM::Name::Vic20DanishKernel;
			break;
			case Region::Japanese:
				character = ROM::Name::Vic20JapaneseCharacters;
				kernel = ROM::Name::Vic20JapaneseKernel;
			break;
			case Region::Swedish:
				character = ROM::Name::Vic20SwedishCharacters;
				kernel = ROM::Name::Vic20SwedishKernel;
			break;
		}

		if(target.has_c1540) {
			request = request && Commodore::C1540::Machine::rom_request(Commodore::C1540::Personality::C1540);
		}
		request = request && ROM::Request(character) && ROM::Request(kernel);

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		basic_rom_ = std::move(roms.find(ROM::Name::Vic20BASIC)->second);
		character_rom_ = std::move(roms.find(character)->second);
		kernel_rom_ = std::move(roms.find(kernel)->second);

		if(target.has_c1540) {
			// construct the 1540
			c1540_ = std::make_unique<C1540::Machine>(C1540::Personality::C1540, roms);

			// attach it to the serial bus
			c1540_->set_serial_bus(serial_bus_);

			// give it a little warm up
			c1540_->run_for(Cycles(2000000));
		}

		// Determine PAL/NTSC.
		if(target.region == Region::American || target.region == Region::Japanese) {
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

		// Add 6502-visible RAM as requested.
		const auto set_ram = [&](uint16_t base_address, uint16_t length) {
			write_to_map(processor_read_memory_map_, &ram_[base_address], base_address, length);
			write_to_map(processor_write_memory_map_, &ram_[base_address], base_address, length);
		};
		set_ram(0x0000, 0x0400);
		set_ram(0x1000, 0x1000);	// Built-in RAM.
		if(target.enabled_ram.bank0) set_ram(0x0400, 0x0c00);	// Bank 0:	0x0400 -> 0x1000.
		if(target.enabled_ram.bank1) set_ram(0x2000, 0x2000);	// Bank 1:	0x2000 -> 0x4000.
		if(target.enabled_ram.bank2) set_ram(0x4000, 0x2000);	// Bank 2:	0x4000 -> 0x6000.
		if(target.enabled_ram.bank3) set_ram(0x6000, 0x2000);	// Bank 3:	0x6000 -> 0x8000.
		if(target.enabled_ram.bank5) set_ram(0xa000, 0x2000);	// Bank 5:	0xa000 -> 0xc000.

		// All expansions also have colour RAM visible at 0x9400.
		write_to_map(processor_read_memory_map_, colour_ram_, 0x9400, sizeof(colour_ram_));
		write_to_map(processor_write_memory_map_, colour_ram_, 0x9400, sizeof(colour_ram_));

		// also push memory resources into the 6560 video memory map; the 6560 has only a
		// 14-bit address bus and the top bit is invested and used as bit 15 for the main
		// memory bus. It can access only internal memory, so the first 1kb, then the 4kb from 0x1000.
		struct Range {
			const std::size_t start, end;
		};
		const std::array<Range, 2> video_ranges = {{
			{0x0000, 0x0400},
			{0x1000, 0x2000},
		}};
		for(const auto &video_range : video_ranges) {
			for(auto addr = video_range.start; addr < video_range.end; addr += 0x400) {
				auto destination_address = (addr & 0x1fff) | (((addr & 0x8000) >> 2) ^ 0x2000);
				if(processor_read_memory_map_[addr >> 10]) {
					write_to_map(
						mos6560_bus_handler_.video_memory_map, &ram_[addr], uint16_t(destination_address), 0x400);
				}
			}
		}
		mos6560_bus_handler_.colour_memory = colour_ram_;

		// Install ROMs.
		write_to_map(processor_read_memory_map_, basic_rom_.data(), 0xc000, basic_rom_.size());
		write_to_map(processor_read_memory_map_, character_rom_.data(), 0x8000, character_rom_.size());
		write_to_map(mos6560_bus_handler_.video_memory_map, character_rom_.data(), 0x0000, character_rom_.size());
		write_to_map(processor_read_memory_map_, kernel_rom_.data(), 0xe000, kernel_rom_.size());

		// Insert media last so that if there's a conflict between cartridges and RAM, the cartridge wins.
		insert_media(target.media);
		if(!target.loading_command.empty()) {
			type_string(target.loading_command);
		}
	}

	bool insert_media(const Analyser::Static::Media &media) final {
		if(!media.tapes.empty()) {
			tape_->set_tape(media.tapes.front(), TargetPlatform::Vic20);
		}

		if(!media.disks.empty() && c1540_) {
			c1540_->set_disk(media.disks.front());
		}

		if(!media.cartridges.empty()) {
			rom_address_ = 0xa000;
			std::vector<uint8_t> rom_image = media.cartridges.front()->get_segments().front().data;
			rom_length_ = uint16_t(rom_image.size());

			rom_ = rom_image;
			rom_.resize(0x2000);
			write_to_map(processor_read_memory_map_, rom_.data(), rom_address_, rom_length_);
		}

		set_use_fast_tape();
		return !media.tapes.empty() || (!media.disks.empty() && c1540_ != nullptr) || !media.cartridges.empty();
	}

	void set_key_state(const uint16_t key, const bool is_pressed) final {
		if(key < KeyUp) {
			keyboard_via_port_handler_.set_key_state(key, is_pressed);
		} else {
			switch(key) {
				case KeyRestore:
					user_port_via_.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(!is_pressed);
				break;
#define ShiftedMap(source, target)	\
				case source:	\
					keyboard_via_port_handler_.set_key_state(KeyLShift, is_pressed);	\
					keyboard_via_port_handler_.set_key_state(target, is_pressed);	\
				break;

				ShiftedMap(KeyUp, KeyDown);
				ShiftedMap(KeyLeft, KeyRight);
				ShiftedMap(KeyF2, KeyF1);
				ShiftedMap(KeyF4, KeyF3);
				ShiftedMap(KeyF6, KeyF5);
				ShiftedMap(KeyF8, KeyF7);
#undef ShiftedMap
			}
		}
	}

	void clear_all_keys() final {
		keyboard_via_port_handler_.clear_all_keys();
		set_key_state(KeyRestore, false);
	}

	const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
		return joysticks_;
	}

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		// Tun the phase-1 part of this cycle, in which the VIC accesses memory.
		cycles_since_mos6560_update_++;

		// Run the phase-2 part of the cycle, which is whatever the 6502 said it should be.
		const auto is_from_rom = [&]() {
			return m6502_.registers().pc.full > 0x8000;
		};
		if constexpr (is_read(operation)) {
			const auto page = processor_read_memory_map_[address >> 10];
			uint8_t result;
			if(!page) {
				if(!is_from_rom()) confidence_.add_miss();
				result = 0xff;
			} else {
				result = processor_read_memory_map_[address >> 10][address & 0x3ff];
			}
			if((address&0xfc00) == 0x9000) {
				if(!(address&0x100)) {
					update_video();
					result &= mos6560_.read(address);
				}
				if(address & 0x10) result &= user_port_via_.read(address);
				if(address & 0x20) result &= keyboard_via_.read(address);

				if(!is_from_rom()) {
					if((address & 0x100) && !(address & 0x30)) {
						confidence_.add_miss();
					} else {
						confidence_.add_hit();
					}
				}
			}
			value = result;

			// Consider applying the fast tape hack.
			if(use_fast_tape_hack_ && operation == CPU::MOS6502Mk2::BusOperation::ReadOpcode) {
				if(address == 0xf7b2) {
					// Address 0xf7b2 contains a JSR to 0xf8c0 ('RDTPBLKS') that will fill the tape buffer with the
					// next header. Skip that via a three-byte NOP and fill in the next header programmatically.
					Storage::Tape::Commodore::Parser parser(TargetPlatform::Vic20);
					std::unique_ptr<Storage::Tape::Commodore::Header> header = parser.get_next_header(*tape_->serialiser());

					const auto tape_position = tape_->serialiser()->offset();
					if(header) {
						// serialise to wherever b2:b3 points
						const uint16_t tape_buffer_pointer = uint16_t(ram_[0xb2]) | uint16_t(ram_[0xb3] << 8);
						header->serialise(&ram_[tape_buffer_pointer], 0x8000 - tape_buffer_pointer);
						hold_tape_ = true;
						Logger::info().append("Found header");
					} else {
						// no header found, so pretend this hack never interceded
						tape_->serialiser()->set_offset(tape_position);
						hold_tape_ = false;
						Logger::info().append("Didn't find header");
					}

					// clear status and the verify flag
					ram_[0x90] = 0;
					ram_[0x93] = 0;

					value = 0x0c;	// i.e. NOP abs, to swallow the entire JSR
				} else if(address == 0xf90b) {
					auto registers = m6502_.registers();
					if(registers.x == 0xe) {
						Storage::Tape::Commodore::Parser parser(TargetPlatform::Vic20);
						const auto tape_position = tape_->serialiser()->offset();
						const std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(*tape_->serialiser());
						if(data) {
							uint16_t start_address, end_address;
							start_address = uint16_t(ram_[0xc1] | (ram_[0xc2] << 8));
							end_address = uint16_t(ram_[0xae] | (ram_[0xaf] << 8));

							// perform a via-processor_write_memory_map_ memcpy
							uint8_t *data_ptr = data->data.data();
							std::size_t data_left = data->data.size();
							while(data_left && start_address != end_address) {
								uint8_t *const tape_page = processor_write_memory_map_[start_address >> 10];
								if(tape_page) tape_page[start_address & 0x3ff] = *data_ptr;
								data_ptr++;
								start_address++;
								data_left--;
							}

							// set tape status, carry and flag
							ram_[0x90] |= 0x40;
							registers.flags.set_per<CPU::MOS6502Mk2::Flag::Carry>(0);
							registers.flags.set_per<CPU::MOS6502Mk2::Flag::Interrupt>(0);

							// to ensure that execution proceeds to 0xfccf, pretend a NOP was here and
							// ensure that the PC leaps to 0xfccf
							registers.pc.full = 0xfccf;
							value = 0xea;	// i.e. NOP implied
							hold_tape_ = true;
							Logger::info().append("Found data");
						} else {
							tape_->serialiser()->set_offset(tape_position);
							hold_tape_ = false;
							Logger::info().append("Didn't find data");
						}
					}
					m6502_.set_registers(registers);
				}
			}
		} else {
			uint8_t *const ram = processor_write_memory_map_[address >> 10];
			if(ram) {
				update_video();
				ram[address & 0x3ff] = value;
			}
			// Anything between 0x9000 and 0x9400 is the IO area.
			if((address&0xfc00) == 0x9000) {
				// The VIC is selected by bit 8 = 0
				if(!(address&0x100)) {
					update_video();
					mos6560_.write(address, value);
				}
				// The first VIA is selected by bit 4 = 1.
				if(address & 0x10) user_port_via_.write(address, value);
				// The second VIA is selected by bit 5 = 1.
				if(address & 0x20) keyboard_via_.write(address, value);

				if(!is_from_rom()) {
					if((address & 0x100) && !(address & 0x30)) {
						confidence_.add_miss();
					} else {
						confidence_.add_hit();
					}
				}
			} else if(!ram) {
				if(!is_from_rom()) confidence_.add_miss();
			}
		}

		user_port_via_.run_for(Cycles(1));
		keyboard_via_.run_for(Cycles(1));
		if(typer_ && address == 0xeb1e && operation == CPU::MOS6502Mk2::BusOperation::ReadOpcode) {
			if(!typer_->type_next_character()) {
				clear_all_keys();
				typer_.reset();
			}
		}
		if(!tape_is_sleeping_ && !hold_tape_) tape_->run_for(Cycles(1));
		if(c1540_) c1540_->run_for(Cycles(1));

		return Cycles(1);
	}

	void flush_output(const int outputs) final {
		if(outputs & Output::Video) {
			update_video();
		}
		if(outputs & Output::Audio) {
			mos6560_.flush();
		}
	}

	void run_for(const Cycles cycles) final {
		m6502_.run_for(cycles);
	}

	void set_scan_target(Outputs::Display::ScanTarget *const scan_target) final {
		mos6560_.set_scan_target(scan_target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return mos6560_.get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) final {
		mos6560_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const final {
		return mos6560_.get_display_type();
	}

	Outputs::Speaker::Speaker *get_speaker() final {
		return mos6560_.get_speaker();
	}

	void mos6522_did_change_interrupt_status(void *) final {
		m6502_.template set<CPU::MOS6502Mk2::Line::NMI>(user_port_via_.get_interrupt_line());
		m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(keyboard_via_.get_interrupt_line());
	}

	void type_string(const std::string &string) final {
		Utility::TypeRecipient<CharacterMapper>::add_typer(string);
	}

	bool can_type(const char c) const final {
		return Utility::TypeRecipient<CharacterMapper>::can_type(c);
	}

	void tape_did_change_input(Storage::Tape::BinaryTapePlayer &tape) final {
		keyboard_via_.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(!tape.input());
	}

	KeyboardMapper *get_keyboard_mapper() final {
		return &keyboard_mapper_;
	}

	// MARK: - Configuration options.
	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->output = get_video_signal_configurable();
		options->quick_load = allow_fast_tape_hack_;
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		set_video_signal_configurable(options->output);
		allow_fast_tape_hack_ = options->quick_load;
		set_use_fast_tape();
	}

	void set_component_prefers_clocking(ClockingHint::Source *, const ClockingHint::Preference clocking) final {
		tape_is_sleeping_ = clocking == ClockingHint::Preference::None;
		set_use_fast_tape();
	}

	// MARK: - Activity Source
	void set_activity_observer(Activity::Observer *const observer) final {
		if(c1540_) c1540_->set_activity_observer(observer);
	}

private:
	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::M6502, M6502Traits> m6502_;

	void update_video() {
		mos6560_.run_for(cycles_since_mos6560_update_.flush<Cycles>());
	}

	std::vector<uint8_t> character_rom_;
	std::vector<uint8_t> basic_rom_;
	std::vector<uint8_t> kernel_rom_;

	std::vector<uint8_t> rom_;
	uint16_t rom_address_, rom_length_;
	uint8_t ram_[0x10000];
	uint8_t colour_ram_[0x0400];

	const uint8_t *processor_read_memory_map_[64]{};
	uint8_t *processor_write_memory_map_[64]{};

	void write_to_map(const std::function<void(uint16_t, size_t)> &store, uint16_t address, size_t length) {
		address >>= 10;
		length >>= 10;
		size_t offset = 0;
		while(length--) {
			store(address, offset);
			offset += 0x400;
			++address;
		}
	}
	void write_to_map(
		const uint8_t **const map,
		const uint8_t *const area,
		const uint16_t address,
		const size_t length
	) {
		write_to_map([&](const uint16_t address, const size_t offset) {
			map[address] = &area[offset];
		}, address, length);
	}
	void write_to_map(
		uint8_t **const map,
		uint8_t *const area,
		const uint16_t address,
		const size_t length
	) {
		write_to_map([&](const uint16_t address, const size_t offset) {
			map[address] = &area[offset];
		}, address, length);
	}

	Commodore::Vic20::KeyboardMapper keyboard_mapper_;
	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

	Cycles cycles_since_mos6560_update_;
	Vic6560BusHandler mos6560_bus_handler_;
	MOS::MOS6560::MOS6560<Vic6560BusHandler> mos6560_;
	UserPortVIA user_port_via_port_handler_;
	KeyboardVIA keyboard_via_port_handler_;
	SerialPort serial_port_;
	Serial::Bus serial_bus_;

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
	std::unique_ptr<::Commodore::C1540::Machine> c1540_;

	// MARK: - Confidence.
	Analyser::Dynamic::ConfidenceCounter confidence_;
	float get_confidence() final { return confidence_.confidence(); }
	std::string debug_type() final {
		return "Vic20";
	}
};

}

using namespace Commodore::Vic20;

std::unique_ptr<Machine> Machine::Vic20(
	const Analyser::Static::Target *const target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Commodore::Vic20Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return std::make_unique<Vic20::ConcreteMachine>(*commodore_target, rom_fetcher);
}
