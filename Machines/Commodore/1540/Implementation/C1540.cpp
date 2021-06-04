//
//  Commodore1540.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "../C1540.hpp"

#include <cassert>
#include <cstring>
#include <string>

#include "../../../../Storage/Disk/Encodings/CommodoreGCR.hpp"

using namespace Commodore::C1540;

ROM::Request MachineBase::rom_request(Personality personality) {
	switch(personality) {
		case Personality::C1540:	return ROM::Request(ROM::Name::Commodore1540);
		case Personality::C1541:	return ROM::Request(ROM::Name::Commodore1541);
	}
}

MachineBase::MachineBase(Personality personality, const ROM::Map &roms) :
		Storage::Disk::Controller(1000000),
		m6502_(*this),
		serial_port_VIA_port_handler_(new SerialPortVIA(serial_port_VIA_)),
		serial_port_(new SerialPort),
		drive_VIA_(drive_VIA_port_handler_),
		serial_port_VIA_(*serial_port_VIA_port_handler_) {
	// attach the serial port to its VIA and vice versa
	serial_port_->set_serial_port_via(serial_port_VIA_port_handler_);
	serial_port_VIA_port_handler_->set_serial_port(serial_port_);

	// set this instance as the delegate to receive interrupt requests from both VIAs
	serial_port_VIA_port_handler_->set_interrupt_delegate(this);
	drive_VIA_port_handler_.set_interrupt_delegate(this);
	drive_VIA_port_handler_.set_delegate(this);

	// set a bit rate
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(3));

	// attach the only drive there is
	emplace_drive(1000000, 300, 2);
	set_drive(1);

	ROM::Name rom_name;
	switch(personality) {
		case Personality::C1540:	rom_name = ROM::Name::Commodore1540;	break;
		case Personality::C1541:	rom_name = ROM::Name::Commodore1541;	break;
	}

	auto rom = roms.find(rom_name);
	if(rom == roms.end()) {
		throw ROMMachine::Error::MissingROMs;
	}
	std::memcpy(rom_, roms.find(rom_name)->second.data(), std::min(sizeof(rom_), roms.find(rom_name)->second.size()));
}

Machine::Machine(Personality personality, const ROM::Map &roms) :
	MachineBase(personality, roms) {}

void Machine::set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus) {
	Commodore::Serial::AttachPortAndBus(serial_port_, serial_bus);
}

Cycles MachineBase::perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	/*
		Memory map (given that I'm unsure yet on any potential mirroring):

			0x0000-0x07ff	RAM
			0x1800-0x180f	the serial-port VIA
			0x1c00-0x1c0f	the drive VIA
			0xc000-0xffff	ROM
	*/
	if(address < 0x800) {
		if(isReadOperation(operation))
			*value = ram_[address];
		else
			ram_[address] = *value;
	} else if(address >= 0xc000) {
		if(isReadOperation(operation)) {
			*value = rom_[address & 0x3fff];
		}
	} else if(address >= 0x1800 && address <= 0x180f) {
		if(isReadOperation(operation))
			*value = serial_port_VIA_.read(address);
		else
			serial_port_VIA_.write(address, *value);
	} else if(address >= 0x1c00 && address <= 0x1c0f) {
		if(isReadOperation(operation))
			*value = drive_VIA_.read(address);
		else
			drive_VIA_.write(address, *value);
	}

	serial_port_VIA_.run_for(Cycles(1));
	drive_VIA_.run_for(Cycles(1));

	return Cycles(1);
}

void Machine::set_disk(std::shared_ptr<Storage::Disk::Disk> disk) {
	get_drive().set_disk(disk);
}

void Machine::run_for(const Cycles cycles) {
	m6502_.run_for(cycles);

	const bool drive_motor = drive_VIA_port_handler_.get_motor_enabled();
	get_drive().set_motor_on(drive_motor);
	if(drive_motor)
		Storage::Disk::Controller::run_for(cycles);
}

void MachineBase::set_activity_observer(Activity::Observer *observer) {
	drive_VIA_.bus_handler().set_activity_observer(observer);
	get_drive().set_activity_observer(observer, "Drive", false);
}

// MARK: - 6522 delegate

void MachineBase::mos6522_did_change_interrupt_status(void *) {
	// both VIAs are connected to the IRQ line
	m6502_.set_irq_line(serial_port_VIA_.get_interrupt_line() || drive_VIA_.get_interrupt_line());
}

// MARK: - Disk drive

void MachineBase::process_input_bit(int value) {
	shift_register_ = (shift_register_ << 1) | value;
	if((shift_register_ & 0x3ff) == 0x3ff) {
		drive_VIA_port_handler_.set_sync_detected(true);
		bit_window_offset_ = -1; // i.e. this bit isn't the first within a data window, but the next might be
	} else {
		drive_VIA_port_handler_.set_sync_detected(false);
	}
	bit_window_offset_++;
	if(bit_window_offset_ == 8) {
		drive_VIA_port_handler_.set_data_input(uint8_t(shift_register_));
		bit_window_offset_ = 0;
		if(drive_VIA_port_handler_.get_should_set_overflow()) {
			m6502_.set_overflow_line(true);
		}
	}
	else m6502_.set_overflow_line(false);
}

// the 1540 does not recognise index holes
void MachineBase::process_index_hole()	{}

// MARK: - Drive VIA delegate

void MachineBase::drive_via_did_step_head(void *, int direction) {
	get_drive().step(Storage::Disk::HeadPosition(direction, 2));
}

void MachineBase::drive_via_did_set_data_density(void *, int density) {
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned(density)));
}

// MARK: - SerialPortVIA

SerialPortVIA::SerialPortVIA(MOS::MOS6522::MOS6522<SerialPortVIA> &via) : via_(via) {}

uint8_t SerialPortVIA::get_port_input(MOS::MOS6522::Port port) {
	if(port) return port_b_;
	return 0xff;
}

void SerialPortVIA::set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t) {
	if(port) {
		std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
		if(serialPort) {
			attention_acknowledge_level_ = !(value&0x10);
			data_level_output_ = (value&0x02);

			serialPort->set_output(::Commodore::Serial::Line::Clock, ::Commodore::Serial::LineLevel(!(value&0x08)));
			update_data_line();
		}
	}
}

void SerialPortVIA::set_serial_line_state(::Commodore::Serial::Line line, bool value) {
	switch(line) {
		default: break;
		case ::Commodore::Serial::Line::Data:		port_b_ = (port_b_ & ~0x01) | (value ? 0x00 : 0x01);		break;
		case ::Commodore::Serial::Line::Clock:		port_b_ = (port_b_ & ~0x04) | (value ? 0x00 : 0x04);		break;
		case ::Commodore::Serial::Line::Attention:
			attention_level_input_ = !value;
			port_b_ = (port_b_ & ~0x80) | (value ? 0x00 : 0x80);
			via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !value);
			update_data_line();
		break;
	}
}

void SerialPortVIA::set_serial_port(const std::shared_ptr<::Commodore::Serial::Port> &serialPort) {
	serial_port_ = serialPort;
}

void SerialPortVIA::update_data_line() {
	std::shared_ptr<::Commodore::Serial::Port> serialPort = serial_port_.lock();
	if(serialPort) {
		// "ATN (Attention) is an input on pin 3 of P2 and P3 that is sensed at PB7 and CA1 of UC3 after being inverted by UA1"
		serialPort->set_output(::Commodore::Serial::Line::Data,
			::Commodore::Serial::LineLevel(!data_level_output_ && (attention_level_input_ != attention_acknowledge_level_)));
	}
}

// MARK: - DriveVIA

void DriveVIA::set_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

// write protect tab uncovered
uint8_t DriveVIA::get_port_input(MOS::MOS6522::Port port) {
	return port ? port_b_ : port_a_;
}

void DriveVIA::set_sync_detected(bool sync_detected) {
	port_b_ = (port_b_ & 0x7f) | (sync_detected ? 0x00 : 0x80);
}

void DriveVIA::set_data_input(uint8_t value) {
	port_a_ = value;
}

bool DriveVIA::get_should_set_overflow() {
	return should_set_overflow_;
}

bool DriveVIA::get_motor_enabled() {
	return drive_motor_;
}

void DriveVIA::set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
	if(port == MOS::MOS6522::Port::A && line == MOS::MOS6522::Line::Two) {
		should_set_overflow_ = value;
	}
}

void DriveVIA::set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t) {
	if(port) {
		if(previous_port_b_output_ != value) {
			// record drive motor state
			drive_motor_ = !!(value&4);

			// check for a head step
			int step_difference = ((value&3) - (previous_port_b_output_&3))&3;
			if(step_difference) {
				if(delegate_) delegate_->drive_via_did_step_head(this, (step_difference == 1) ? 1 : -1);
			}

			// check for a change in density
			int density_difference = (previous_port_b_output_^value) & (3 << 5);
			if(density_difference && delegate_) {
				delegate_->drive_via_did_set_data_density(this, (value >> 5)&3);
			}

			// post the LED status
			if(observer_) observer_->set_led_status("Drive", !!(value&8));

			previous_port_b_output_ = value;
		}
	}
}

void DriveVIA::set_activity_observer(Activity::Observer *observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("Drive");
		observer->set_led_status("Drive", !!(previous_port_b_output_&8));
	}
}

// MARK: - SerialPort

void SerialPort::set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level) {
	std::shared_ptr<SerialPortVIA> serialPortVIA = serial_port_VIA_.lock();
	if(serialPortVIA) serialPortVIA->set_serial_line_state(line, bool(level));
}

void SerialPort::set_serial_port_via(const std::shared_ptr<SerialPortVIA> &serialPortVIA) {
	serial_port_VIA_ = serialPortVIA;
}
