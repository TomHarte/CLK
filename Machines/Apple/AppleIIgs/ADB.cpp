//
//  ADB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "ADB.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

// TEST.
#include "../../../InstructionSets/M50740/Parser.hpp"
#include "../../../InstructionSets/Disassembler.hpp"

using namespace Apple::IIgs::ADB;

namespace {

// Flags affecting the CPU-visible status register.
enum class CPUFlags: uint8_t {
	MouseDataFull = 0x80,
	MouseInterruptEnabled = 0x40,
	CommandDataIsValid = 0x20,
	CommandDataInterruptEnabled = 0x10,
	KeyboardDataFull = 0x08,
	KeyboardDataInterruptEnabled = 0x04,
	MouseXIsAvailable = 0x02,
	CommandRegisterFull = 0x01,
};

// Flags affecting the microcontroller-visible register.
enum class MicrocontrollerFlags: uint8_t {
	CommandRegisterFull = 0x40,
};

}

GLU::GLU() :
	executor_(*this),
	bus_(HalfCycles(1'789'772)),
	controller_id_(bus_.add_device()),
	mouse_(bus_),
	keyboard_(bus_) {}

// MARK: - External interface.

uint8_t GLU::get_keyboard_data() {
	// The classic Apple II serial keyboard register:
	// b7:		key strobe.
	// b6–b0:	ASCII code.
	return registers_[0];
}

void GLU::clear_key_strobe() {
	// Clears the key strobe of the classic Apple II serial keyboard register.
	registers_[0] &= 0x7f;	// ???
}

uint8_t GLU::get_any_key_down() {
	// The Apple IIe check-for-any-key-down bit.
	return registers_[5];
}

uint8_t GLU::get_mouse_data() {
	// Alternates between returning x and y values.
	//
	// b7: 		1 = button is up; 0 = button is down.
	// b6:		delta sign bit; 1 = negative.
	// b5–b0:	mouse delta.
	return 0x80;	// TODO. Should alternate between registers 2 and 3.
}

uint8_t GLU::get_modifier_status() {
	// b7:		1 = command key pressed; 0 = not.
	// b6:		option key.
	// b5:		1 = modifier key latch has been updated, no key has been pressed; 0 = not.
	// b4:		any numeric keypad key.
	// b3:		a key is down.
	// b2:		caps lock is pressed.
	// b1:		control key.
	// b0:		shift key.
	return registers_[6];
}

uint8_t GLU::get_data() {
	// b0–2:	number of data bytes to be returned.
	// b3:		1 = a valid service request is pending; 0 = no request pending.
	// b4:		1 = control, command and delete keys have been pressed simultaneously; 0 = they haven't.
	// b5:		1 = control, command and reset have all been pressed together; 0 = they haven't.
	// b6:		1 = ADB controller encountered an error and reset itself; 0 = no error.
	// b7:		1 = ADB has received a response from the addressed ADB device; 0 = no respone.
//	status_ &= ~(CPUFlags::CommandDataIsValid | CPUFlags::CommandRegisterFull);
	return registers_[7];
}

uint8_t GLU::get_status() {
	// b7:	1 = mouse data register is full; 0 = empty.
	// b6:	1 = mouse interrupt is enabled.
	// b5:	1 = command/data has valid data.
	// b4:	1 = command/data interrupt is enabled.
	// b3:	1 = keyboard data is full.
	// b2:	1 = keyboard data interrupt is enabled.
	// b1:	1 = mouse x-data is available; 0 = y.
	// b0:	1 = command register is full (set when command is written); 0 = empty (cleared when data is read).
	return status_;
}

void GLU::set_status(uint8_t status) {
	// This permits only the interrupt flags to be set.
	constexpr uint8_t interrupt_flags =
		uint8_t(CPUFlags::MouseInterruptEnabled) |
		uint8_t(CPUFlags::CommandDataInterruptEnabled) |
		uint8_t(CPUFlags::KeyboardDataInterruptEnabled);
	status_ = (status_ & ~interrupt_flags) | (status & interrupt_flags);
}

void GLU::set_command(uint8_t command) {
	registers_[1] = command;
	registers_[4] |= uint8_t(MicrocontrollerFlags::CommandRegisterFull);
	status_ |= uint8_t(CPUFlags::CommandRegisterFull);
}

// MARK: - Setup and run.

void GLU::set_microcontroller_rom(const std::vector<uint8_t> &rom) {
	executor_.set_rom(rom);

	// TEST invocation.
/*	InstructionSet::Disassembler<InstructionSet::M50740::Parser, 0x1fff, InstructionSet::M50740::Instruction, uint8_t, uint16_t> disassembler;
	disassembler.disassemble(rom.data(), 0x1000, uint16_t(rom.size()), 0x1000);

	const auto instructions = disassembler.instructions();
	const auto entry_points = disassembler.entry_points();
	for(const auto &pair : instructions) {
		std::cout << std::hex << pair.first << "\t\t";
		if(entry_points.find(pair.first) != entry_points.end()) {
			std::cout << "L" << pair.first << "\t";
		} else {
			std::cout << "\t\t";
		}
		std::cout << operation_name(pair.second.operation) << " ";
		std::cout << address(pair.second.addressing_mode, &rom[pair.first - 0x1000], pair.first);

		std::cout << std::endl;
	}*/
}

void GLU::run_for(Cycles cycles) {
	executor_.run_for(cycles);
}

// MARK: - M50470 port handler

void GLU::set_port_output(int port, uint8_t value) {
	switch(port) {
		case 0:
			register_latch_ = value;
		break;
		case 1:
//			printf("Keyboard write: %02x???\n", value);
		break;
		case 2: {
//			printf("ADB data line input: %d???\n", value >> 7);
//			printf("IIe keyboard reset line: %d\n", (value >> 6)&1);
//			printf("IIgs reset line: %d\n", (value >> 5)&1);
//			printf("GLU strobe: %d\n", (value >> 4)&1);
//			printf("Select GLU register: %d [%02x]\n", value & 0xf, value);

			register_address_ = value & 0xf;

			// Check the strobe; I think:
			//
			//	high -> low, read => fill latch from register;
			//	low -> high => fill register from latch.
			const bool strobe = value & 0x10;
			if(strobe != register_strobe_) {
				register_strobe_ = strobe;

				if(register_strobe_) {
					registers_[register_address_] = register_latch_;
					switch(register_address_) {
						default: break;
						case 7:		status_ |= uint8_t(CPUFlags::CommandDataIsValid);	break;
					}
				} else {
					register_latch_ = registers_[register_address_];
					switch(register_address_) {
						default: break;
						case 1:
							registers_[4] &= ~uint8_t(MicrocontrollerFlags::CommandRegisterFull);
							status_ &= ~uint8_t(CPUFlags::CommandRegisterFull);
						break;
					}
				}
			}
		} break;
		case 3:
			modifier_state_ = value;

			// Output is inverted respective to input; the microcontroller
			// sets a value of '1' in order to pull the ADB bus low.
			bus_.set_device_output(controller_id_, !(value & 0x08));
		break;

		default: assert(false);
	}
}

bool GLU::get_command_button() const {
	return modifier_state_ & 0x20;
}

bool GLU::get_option_button() const {
	return modifier_state_ & 0x10;
}

uint8_t GLU::get_port_input(int port) {
	switch(port) {
		case 0:	return register_latch_;
		case 1:
//			printf("IIe keyboard read\n");
		return 0x06;
		case 2:
//			printf("ADB data line input, etc\n");
		return bus_.get_state() ? 0x80 : 0x00;
		case 3:
//			printf("ADB data line output, etc\n");
		return 0x00;

		default: assert(false);
	}
	return 0xff;
}

void GLU::run_ports_for(Cycles cycles) {
	bus_.run_for(cycles);
}
