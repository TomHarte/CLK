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

GLU::GLU() : executor_(*this) {}

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

void GLU::set_command(uint8_t command) {
	registers_[1] = command;
	registers_[4] |= uint8_t(MicrocontrollerFlags::CommandRegisterFull);
	status_ |= uint8_t(CPUFlags::CommandRegisterFull);
//	printf("!!!%02x!!!\n", command);
}

void GLU::set_status(uint8_t status) {
	printf("TODO: set ADB status %02x\n", status);
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
	ports_[port] = value;
	switch(port) {
		case 0:
//			printf(" {R%d} ", register_address_);
//			printf("Set R%d: %02x\n", register_address_, value);
			registers_[register_address_] = value;
			switch(register_address_) {
				default: break;
				case 7:				status_ |= uint8_t(CPUFlags::CommandDataIsValid);	break;
			}
		break;
		case 1:
//			printf("Keyboard write: %02x???\n", value);
		break;
		case 2:
//			printf("ADB data line input: %d???\n", value >> 7);
//			printf("IIe keyboard reset line: %d\n", (value >> 6)&1);
//			printf("IIgs reset line: %d\n", (value >> 5)&1);
//			printf("GLU strobe: %d\n", (value >> 4)&1);
//			printf("Select GLU register: %d [%02x]\n", value & 0xf, value);
			register_address_ = value & 0xf;
		break;
		case 3: {
//			printf("IIe KWS: %d\n", (value >> 6)&3);
//			printf("ADB data line output: %d\n", (value >> 3)&1);

			const bool new_adb_level = !(value & 0x08);
			if(new_adb_level != adb_level_) {
				printf(".");
				if(!new_adb_level) {
					// Transition to low.
					constexpr float clock_rate = 894886.25;
					const float seconds = float(total_period_.as<int>()) / clock_rate;

					// Check for a valid bit length — 70 to 130 microseconds.
					// (Plus a little).
					if(seconds >= 0.000'56 && seconds <= 0.001'04) {
						printf("!!! Attention\n");
					} else if(seconds >= 0.000'06 && seconds <= 0.000'14) {
						printf("!!! bit: %d\n", (low_period_.as<int>() * 2) < total_period_.as<int>());
//						printf("tested: %0.2f\n", float(low_period_.as<int>()) / float(total_period_.as<int>()));
					} else {
						printf("!!! Rejected %d microseconds\n", int(seconds * 1'000'000.0f));
					}

					total_period_ = low_period_ = Cycles(0);
				}
				adb_level_ = new_adb_level;
			}
		} break;

		default: assert(false);
	}
}

uint8_t GLU::get_port_input(int port) {
	switch(port) {
		case 0:
//			printf(" {R%d} ", register_address_);
			switch(register_address_) {
				default: break;
				case 1:
					registers_[4] &= ~uint8_t(MicrocontrollerFlags::CommandRegisterFull);
					status_ &= ~uint8_t(CPUFlags::CommandRegisterFull);
				break;
			}

			if(register_address_ == 1) {
				printf("[C %02x]", registers_[1]);
			}
		return registers_[register_address_];
		case 1:
//			printf("IIe keyboard read\n");
		return 0x06;
		case 2:
//			printf("ADB data line input, etc\n");
		return ports_[2];
		case 3:
//			printf("ADB data line output, etc\n");
		return ports_[3];

		default: assert(false);
	}
	return 0xff;
}

void GLU::run_ports_for(Cycles cycles) {
	total_period_ += cycles;
	if(!adb_level_) {
		low_period_ += cycles;
	}
}
