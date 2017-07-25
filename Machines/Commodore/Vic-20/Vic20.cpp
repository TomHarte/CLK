//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include <algorithm>
#include "../../../Storage/Tape/Formats/TapePRG.hpp"
#include "../../../Storage/Tape/Parsers/Commodore.hpp"
#include "../../../StaticAnalyser/StaticAnalyser.hpp"

using namespace Commodore::Vic20;

Machine::Machine() :
		rom_(nullptr),
		is_running_at_zero_cost_(false),
		tape_(new Storage::Tape::BinaryTapePlayer(1022727)),
		user_port_via_(new UserPortVIA),
		keyboard_via_(new KeyboardVIA),
		serial_port_(new SerialPort),
		serial_bus_(new ::Commodore::Serial::Bus) {
	// communicate the tape to the user-port VIA
	user_port_via_->set_tape(tape_);

	// wire up the serial bus and serial port
	Commodore::Serial::AttachPortAndBus(serial_port_, serial_bus_);

	// wire up 6522s and serial port
	user_port_via_->set_serial_port(serial_port_);
	keyboard_via_->set_serial_port(serial_port_);
	serial_port_->set_user_port_via(user_port_via_);

	// wire up the 6522s, tape and machine
	user_port_via_->set_interrupt_delegate(this);
	keyboard_via_->set_interrupt_delegate(this);
	tape_->set_delegate(this);

	// establish the memory maps
	set_memory_size(MemorySize::Default);

	// set the NTSC clock rate
	set_region(NTSC);
//	_debugPort.reset(new ::Commodore::Serial::DebugPort);
//	_debugPort->set_serial_bus(serial_bus_);
//	serial_bus_->add_port(_debugPort);
}

void Machine::set_memory_size(MemorySize size) {
	memset(processor_read_memory_map_, 0, sizeof(processor_read_memory_map_));
	memset(processor_write_memory_map_, 0, sizeof(processor_write_memory_map_));

	switch(size) {
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
	write_to_map(processor_read_memory_map_, character_rom_, 0x8000, sizeof(character_rom_));
	write_to_map(processor_read_memory_map_, basic_rom_, 0xc000, sizeof(basic_rom_));
	write_to_map(processor_read_memory_map_, kernel_rom_, 0xe000, sizeof(kernel_rom_));

	write_to_map(processor_write_memory_map_, user_basic_memory_, 0x0000, sizeof(user_basic_memory_));
	write_to_map(processor_write_memory_map_, screen_memory_, 0x1000, sizeof(screen_memory_));
	write_to_map(processor_write_memory_map_, colour_memory_, 0x9400, sizeof(colour_memory_));

	// install the inserted ROM if there is one
	if(rom_) {
		write_to_map(processor_read_memory_map_, rom_, rom_address_, rom_length_);
	}
}

void Machine::write_to_map(uint8_t **map, uint8_t *area, uint16_t address, uint16_t length) {
	address >>= 10;
	length >>= 10;
	while(length--) {
		map[address] = area;
		area += 0x400;
		address++;
	}
}

Machine::~Machine() {
	delete[] rom_;
}

unsigned int Machine::perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	// run the phase-1 part of this cycle, in which the VIC accesses memory
	if(!is_running_at_zero_cost_) mos6560_->run_for_cycles(1);

	// run the phase-2 part of the cycle, which is whatever the 6502 said it should be
	if(isReadOperation(operation)) {
		uint8_t result = processor_read_memory_map_[address >> 10] ? processor_read_memory_map_[address >> 10][address & 0x3ff] : 0xff;
		if((address&0xfc00) == 0x9000) {
			if((address&0xff00) == 0x9000)	result &= mos6560_->get_register(address);
			if((address&0xfc10) == 0x9010)	result &= user_port_via_->get_register(address);
			if((address&0xfc20) == 0x9020)	result &= keyboard_via_->get_register(address);
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
				uint16_t tape_buffer_pointer = (uint16_t)user_basic_memory_[0xb2] | (uint16_t)(user_basic_memory_[0xb3] << 8);
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
				uint8_t x = (uint8_t)get_value_of_register(CPU::MOS6502::Register::X);
				if(x == 0xe) {
					Storage::Tape::Commodore::Parser parser;
					std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(tape_->get_tape());
					uint16_t start_address, end_address;
					start_address = (uint16_t)(user_basic_memory_[0xc1] | (user_basic_memory_[0xc2] << 8));
					end_address = (uint16_t)(user_basic_memory_[0xae] | (user_basic_memory_[0xaf] << 8));

					// perform a via-processor_write_memory_map_ memcpy
					uint8_t *data_ptr = data->data.data();
					size_t data_left = data->data.size();
					while(data_left && start_address != end_address) {
						uint8_t *page = processor_write_memory_map_[start_address >> 10];
						if(page) page[start_address & 0x3ff] = *data_ptr;
						data_ptr++;
						start_address++;
						data_left--;
					}

					// set tape status, carry and flag
					user_basic_memory_[0x90] |= 0x40;
					uint8_t	flags = (uint8_t)get_value_of_register(CPU::MOS6502::Register::Flags);
					flags &= ~(uint8_t)(CPU::MOS6502::Flag::Carry | CPU::MOS6502::Flag::Interrupt);
					set_value_of_register(CPU::MOS6502::Register::Flags, flags);

					// to ensure that execution proceeds to 0xfccf, pretend a NOP was here and
					// ensure that the PC leaps to 0xfccf
					set_value_of_register(CPU::MOS6502::Register::ProgramCounter, 0xfccf);
					*value = 0xea;	// i.e. NOP implied
				}
			}
		}
	} else {
		uint8_t *ram = processor_write_memory_map_[address >> 10];
		if(ram) ram[address & 0x3ff] = *value;
		if((address&0xfc00) == 0x9000) {
			if((address&0xff00) == 0x9000)	mos6560_->set_register(address, *value);
			if((address&0xfc10) == 0x9010)	user_port_via_->set_register(address, *value);
			if((address&0xfc20) == 0x9020)	keyboard_via_->set_register(address, *value);
		}
	}

	user_port_via_->run_for_cycles(1);
	keyboard_via_->run_for_cycles(1);
	if(typer_ && operation == CPU::MOS6502::BusOperation::ReadOpcode && address == 0xEB1E) {
		if(!typer_->type_next_character()) {
			clear_all_keys();
			typer_.reset();
		}
	}
	tape_->run_for(Cycles(1));
	if(c1540_) c1540_->run_for_cycles(1);

	return 1;
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522) {
	set_nmi_line(user_port_via_->get_interrupt_line());
	set_irq_line(keyboard_via_->get_interrupt_line());
}

#pragma mark - Setup

void Machine::set_region(Commodore::Vic20::Region region) {
	region_ = region;
	switch(region) {
		case PAL:
			set_clock_rate(1108404);
			if(mos6560_) {
				mos6560_->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::PAL);
				mos6560_->set_clock_rate(1108404);
			}
		break;
		case NTSC:
			set_clock_rate(1022727);
			if(mos6560_) {
				mos6560_->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::NTSC);
				mos6560_->set_clock_rate(1022727);
			}
		break;
	}
}

void Machine::setup_output(float aspect_ratio) {
	mos6560_.reset(new Vic6560());
	mos6560_->get_speaker()->set_high_frequency_cut_off(1600);	// There is a 1.6Khz low-pass filter in the Vic-20.
	set_region(region_);

	memset(mos6560_->video_memory_map, 0, sizeof(mos6560_->video_memory_map));
	write_to_map(mos6560_->video_memory_map, character_rom_, 0x0000, sizeof(character_rom_));
	write_to_map(mos6560_->video_memory_map, user_basic_memory_, 0x2000, sizeof(user_basic_memory_));
	write_to_map(mos6560_->video_memory_map, screen_memory_, 0x3000, sizeof(screen_memory_));
	mos6560_->colour_memory = colour_memory_;
}

void Machine::close_output() {
	mos6560_ = nullptr;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data) {
	uint8_t *target = nullptr;
	size_t max_length = 0x2000;
	switch(slot) {
		case Kernel:		target = kernel_rom_;								break;
		case Characters:	target = character_rom_;	max_length = 0x1000;	break;
		case BASIC:			target = basic_rom_;								break;
		case Drive:
			drive_rom_.resize(length);
			memcpy(drive_rom_.data(), data, length);
			install_disk_rom();
		return;
	}

	if(target) {
		size_t length_to_copy = std::min(max_length, length);
		memcpy(target, data, length_to_copy);
	}
}

#pragma mar - Tape

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
	if(target.tapes.size()) {
		tape_->set_tape(target.tapes.front());
	}

	if(target.disks.size()) {
		// construct the 1540
		c1540_.reset(new ::Commodore::C1540::Machine);

		// attach it to the serial bus
		c1540_->set_serial_bus(serial_bus_);

		// hand it the disk
		c1540_->set_disk(target.disks.front());

		// install the ROM if it was previously set
		install_disk_rom();
	}

	if(target.cartridges.size()) {
		rom_address_ = 0xa000;
		std::vector<uint8_t> rom_image = target.cartridges.front()->get_segments().front().data;
		rom_length_ = (uint16_t)(rom_image.size());

		rom_ = new uint8_t[0x2000];
		memcpy(rom_, rom_image.data(), rom_image.size());
		write_to_map(processor_read_memory_map_, rom_, rom_address_, 0x2000);
	}

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
}

void Machine::tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape) {
	keyboard_via_->set_control_line_input(KeyboardVIA::Port::A, KeyboardVIA::Line::One, !tape->get_input());
}

#pragma mark - Disc

void Machine::install_disk_rom() {
	if(!drive_rom_.empty() && c1540_) {
		c1540_->set_rom(drive_rom_);
		c1540_->run_for_cycles(2000000);
		drive_rom_.clear();
	}
}

#pragma mark - UserPortVIA

uint8_t UserPortVIA::get_port_input(Port port) {
	if(!port) {
		return port_a_ | (tape_->has_tape() ? 0x00 : 0x40);
	}
	return 0xff;
}

void UserPortVIA::set_control_line_output(Port port, Line line, bool value) {
	if(port == Port::A && line == Line::Two) {
		tape_->set_motor_control(!value);
	}
}

void UserPortVIA::set_serial_line_state(::Commodore::Serial::Line line, bool value) {
	switch(line) {
		default: break;
		case ::Commodore::Serial::Line::Data: port_a_ = (port_a_ & ~0x02) | (value ? 0x02 : 0x00);	break;
		case ::Commodore::Serial::Line::Clock: port_a_ = (port_a_ & ~0x01) | (value ? 0x01 : 0x00);	break;
	}
}

void UserPortVIA::set_joystick_state(JoystickInput input, bool value) {
	if(input != JoystickInput::Right) {
		port_a_ = (port_a_ & ~input) | (value ? 0 : input);
	}
}

void UserPortVIA::set_port_output(Port port, uint8_t value, uint8_t mask) {
	// Line 7 of port A is inverted and output as serial ATN
	if(!port) {
		std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
		if(serialPort)
			serialPort->set_output(::Commodore::Serial::Line::Attention, (::Commodore::Serial::LineLevel)!(value&0x80));
	}
}

UserPortVIA::UserPortVIA() : port_a_(0xbf) {}

void UserPortVIA::set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
	serial_port_ = serialPort;
}

void UserPortVIA::set_tape(std::shared_ptr<Storage::Tape::BinaryTapePlayer> tape) {
	tape_ = tape;
}

#pragma mark - KeyboardVIA

KeyboardVIA::KeyboardVIA() : port_b_(0xff) {
	clear_all_keys();
}

void KeyboardVIA::set_key_state(uint16_t key, bool isPressed) {
	if(isPressed)
		columns_[key & 7] &= ~(key >> 3);
	else
		columns_[key & 7] |= (key >> 3);
}

void KeyboardVIA::clear_all_keys() {
	memset(columns_, 0xff, sizeof(columns_));
}

uint8_t KeyboardVIA::get_port_input(Port port) {
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

void KeyboardVIA::set_port_output(Port port, uint8_t value, uint8_t mask) {
	if(port)
		activation_mask_ = (value & mask) | (~mask);
}

void KeyboardVIA::set_control_line_output(Port port, Line line, bool value) {
	if(line == Line::Two) {
		std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
		if(serialPort) {
			// CB2 is inverted to become serial data; CA2 is inverted to become serial clock
			if(port == Port::A)
				serialPort->set_output(::Commodore::Serial::Line::Clock, (::Commodore::Serial::LineLevel)!value);
			else
				serialPort->set_output(::Commodore::Serial::Line::Data, (::Commodore::Serial::LineLevel)!value);
		}
	}
}

void KeyboardVIA::set_joystick_state(JoystickInput input, bool value) {
	if(input == JoystickInput::Right) {
		port_b_ = (port_b_ & ~input) | (value ? 0 : input);
	}
}

void KeyboardVIA::set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
	serial_port_ = serialPort;
}

#pragma mark - SerialPort

void SerialPort::set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level) {
	std::shared_ptr<UserPortVIA> userPortVIA = user_port_via_.lock();
	if(userPortVIA) userPortVIA->set_serial_line_state(line, (bool)level);
}

void SerialPort::set_user_port_via(std::shared_ptr<UserPortVIA> userPortVIA) {
	user_port_via_ = userPortVIA;
}
