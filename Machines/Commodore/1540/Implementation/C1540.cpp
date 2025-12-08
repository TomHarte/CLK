//
//  Commodore1540.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Machines/Commodore/1540/C1540.hpp"

#include <cassert>
#include <cstring>
#include <string>

#include "Storage/Disk/Encodings/CommodoreGCR.hpp"

using namespace Commodore::C1540;

namespace {

// MARK: - Construction, including ROM requests.

ROM::Name rom_name(const Personality personality) {
	switch(personality) {
		default:
		case Personality::C1540:	return ROM::Name::Commodore1540;
		case Personality::C1541:	return ROM::Name::Commodore1541;
	}
}
}

ROM::Request Machine::rom_request(const Personality personality) {
	return ROM::Request(rom_name(personality));
}

MachineBase::MachineBase(const Personality personality, const ROM::Map &roms) :
		Storage::Disk::Controller(1000000),
		m6502_(*this),
		drive_VIA_(drive_VIA_port_handler_),
		serial_port_VIA_(serial_port_VIA_port_handler_) {
	// Attach the serial port to its VIA and vice versa.
	serial_port_.connect(serial_port_VIA_port_handler_, serial_port_VIA_);
	serial_port_VIA_port_handler_.set_serial_port(serial_port_);

	// Set this instance as the delegate to receive interrupt requests from both VIAs.
	serial_port_VIA_port_handler_.set_interrupt_delegate(this);
	drive_VIA_port_handler_.set_interrupt_delegate(this);
	drive_VIA_port_handler_.set_delegate(this);

	// Set a bit rate.
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(3));

	// Attach the only drive there is.
	emplace_drive(1000000, 300, 2);
	set_drive(1);

	const auto rom = roms.find(rom_name(personality));
	if(rom == roms.end()) {
		throw ROMMachine::Error::MissingROMs;
	}
	std::copy(rom->second.begin(), rom->second.begin() + ptrdiff_t(std::min(sizeof(rom_), rom->second.size())), rom_);
}

Machine::Machine(const Personality personality, const ROM::Map &roms) :
	MachineBase(personality, roms) {}

// MARK: - 6502 bus.

template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
Cycles MachineBase::perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
	/*
		Memory map (given that I'm unsure yet on any potential mirroring):

			0x0000-0x07ff	RAM
			0x1800-0x180f	the serial-port VIA
			0x1c00-0x1c0f	the drive VIA
			0xc000-0xffff	ROM
	*/
	if(address < 0x800) {
		if constexpr (is_read(operation))
			value = ram_[address];
		else
			ram_[address] = value;
	} else if(address >= 0xc000) {
		if constexpr (is_read(operation)) {
			value = rom_[address & 0x3fff];
		}
	} else if(address >= 0x1800 && address <= 0x180f) {
		if constexpr (is_read(operation))
			value = serial_port_VIA_.read(address);
		else
			serial_port_VIA_.write(address, value);
	} else if(address >= 0x1c00 && address <= 0x1c0f) {
		if constexpr (is_read(operation))
			value = drive_VIA_.read(address);
		else
			drive_VIA_.write(address, value);
	} else {
		if constexpr (is_read(operation)) {
			value = 0xff;
		}
	}

	serial_port_VIA_.run_for(Cycles(1));
	drive_VIA_.run_for(Cycles(1));

	return Cycles(1);
}

void Machine::run_for(const Cycles cycles) {
	m6502_.run_for(cycles);
	if(get_drive().get_motor_on()) {
		Storage::Disk::Controller::run_for(cycles);
	}
}

// MARK: - External attachments.

void Machine::set_serial_bus(Commodore::Serial::Bus &serial_bus) {
	Commodore::Serial::attach(serial_port_, serial_bus);
}

void Machine::set_disk(std::shared_ptr<Storage::Disk::Disk> disk) {
	get_drive().set_disk(disk);
	drive_VIA_port_handler_.set_is_read_only(disk->is_read_only());
}

void MachineBase::set_activity_observer(Activity::Observer *const observer) {
	drive_VIA_.bus_handler().set_activity_observer(observer);
	get_drive().set_activity_observer(observer, "Drive", false);
}

// MARK: - 6522 delegate.

void MachineBase::mos6522_did_change_interrupt_status(void *) {
	// both VIAs are connected to the IRQ line
	m6502_.set<CPU::MOS6502Mk2::Line::IRQ>(serial_port_VIA_.get_interrupt_line() || drive_VIA_.get_interrupt_line());
}

// MARK: - Disk drive.

void MachineBase::process_input_bit(const int value) {
	shift_register_ = (shift_register_ << 1) | value;
	if((shift_register_ & 0x3ff) == 0x3ff) {
		drive_VIA_port_handler_.set_sync_detected(true);
		bit_window_offset_ = -1; // i.e. this bit isn't the first within a data window, but the next might be
	} else {
		drive_VIA_port_handler_.set_sync_detected(false);
	}

	++bit_window_offset_;
	if(bit_window_offset_ == 8) {
		drive_VIA_port_handler_.set_data_input(uint8_t(shift_register_));
		bit_window_offset_ = 0;
		if(set_cpu_overflow_) {
			m6502_.set<CPU::MOS6502Mk2::Line::Overflow>(true);
		}
	} else {
		m6502_.set<CPU::MOS6502Mk2::Line::Overflow>(false);
	}
}

void MachineBase::is_writing_final_bit() {
	if(set_cpu_overflow_) {
		m6502_.set<CPU::MOS6502Mk2::Line::Overflow>(true);
	}
}

void MachineBase::process_write_completed() {
	m6502_.set<CPU::MOS6502Mk2::Line::Overflow>(false);
	serialise_shift_output();
}

void MachineBase::serialise_shift_output() {
	auto &drive = get_drive();
	uint8_t value = port_a_output_;
	for(int c = 0; c < 8; c++) {
		drive.write_bit(value & 0x80);
		value <<= 1;
	}
}

// The 1540 does not recognise index holes.
void MachineBase::process_index_hole()	{}

// MARK: - Drive VIA delegate

void MachineBase::drive_via_did_step_head(DriveVIA &, const int direction) {
	get_drive().step(Storage::Disk::HeadPosition(direction, 2));
}

void MachineBase::drive_via_did_set_data_density(DriveVIA &, const int density) {
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned(density)));
}

void MachineBase::drive_via_did_set_drive_motor(DriveVIA &, const bool enabled) {
	get_drive().set_motor_on(enabled);
}

void MachineBase::drive_via_did_set_write_mode(DriveVIA &, const bool enabled) {
	if(enabled) {
		begin_writing(false, true);
	} else {
		end_writing();
	}
}

void MachineBase::drive_via_set_to_shifter_output(DriveVIA &, const uint8_t value) {
	port_a_output_ = value;
}

void MachineBase::drive_via_should_set_cpu_overflow(DriveVIA &, const bool overflow) {
	set_cpu_overflow_ = overflow;
	if(!overflow) {
		m6502_.set<CPU::MOS6502Mk2::Line::Overflow>(false);
	}
}

// MARK: - SerialPortVIA

template <MOS::MOS6522::Port port>
uint8_t SerialPortVIA::get_port_input() const {
	if(port) {
		return port_b_;
	}
	return 0xff;
}

template <MOS::MOS6522::Port port>
void SerialPortVIA::set_port_output(const uint8_t value, uint8_t) {
	if(port) {
		attention_acknowledge_level_ = !(value&0x10);
		data_level_output_ = (value&0x02);

		serial_port_->set_output(::Commodore::Serial::Line::Clock, ::Commodore::Serial::LineLevel(!(value&0x08)));
		update_data_line();
	}
}

void SerialPortVIA::set_serial_line_state(
	const Commodore::Serial::Line line,
	const bool value,
	MOS::MOS6522::MOS6522<SerialPortVIA> &via
) {
	const auto set = [&](const uint8_t mask) {
		port_b_ = (port_b_ & ~mask) | (value ? 0x00 : mask);
	};

	switch(line) {
		default: break;
		case ::Commodore::Serial::Line::Data:		set(0x01);		break;
		case ::Commodore::Serial::Line::Clock:		set(0x04);		break;
		case ::Commodore::Serial::Line::Attention:
			set(0x80);
			attention_level_input_ = !value;
			via.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(!value);
			update_data_line();
		break;
	}
}

void SerialPortVIA::set_serial_port(Commodore::Serial::Port &port) {
	serial_port_ = &port;
}

void SerialPortVIA::update_data_line() {
	// "ATN (Attention) is an input on pin 3 of P2 and P3 that
	// is sensed at PB7 and CA1 of UC3 after being inverted by UA1"
	serial_port_->set_output(
		::Commodore::Serial::Line::Data,
		Serial::LineLevel(!data_level_output_ && (attention_level_input_ != attention_acknowledge_level_))
	);
}

// MARK: - DriveVIA

void DriveVIA::set_delegate(Delegate *const delegate) {
	delegate_ = delegate;
}

// write protect tab uncovered
template <MOS::MOS6522::Port port>
uint8_t DriveVIA::get_port_input() const {
	return port ? port_b_ : port_a_;
}

void DriveVIA::set_sync_detected(const bool sync_detected) {
	port_b_ = (port_b_ & ~0x80) | (sync_detected ? 0x00 : 0x80);
}

void DriveVIA::set_is_read_only(const bool is_read_only) {
	port_b_ = (port_b_ & ~0x10) | (is_read_only ? 0x00 : 0x10);
}

void DriveVIA::set_data_input(const uint8_t value) {
	port_a_ = value;
}

template <MOS::MOS6522::Port port, MOS::MOS6522::Line line>
void DriveVIA::set_control_line_output(const bool value) {
	if(port == MOS::MOS6522::Port::A && line == MOS::MOS6522::Line::Two) {
		if(set_cpu_overflow_ != value) {
			set_cpu_overflow_ = value;
			if(delegate_) {
				delegate_->drive_via_should_set_cpu_overflow(*this, set_cpu_overflow_);
			}
		}
	}

	if(port == MOS::MOS6522::Port::B && line == MOS::MOS6522::Line::Two) {
		const bool new_write_mode = !value;
		if(new_write_mode != write_mode_) {
			write_mode_ = new_write_mode;
			if(delegate_) {
				delegate_->drive_via_did_set_write_mode(*this, write_mode_);
			}
		}
	}
}

template <>
void DriveVIA::set_port_output<MOS::MOS6522::Port::B>(const uint8_t value, uint8_t) {
	if(previous_port_b_output_ != value) {
		// Record drive motor state.
		const bool new_drive_motor = value & 4;
		if(new_drive_motor != drive_motor_) {
			drive_motor_ = new_drive_motor;
			if(delegate_) {
				delegate_->drive_via_did_set_drive_motor(*this, drive_motor_);
			}
		}

		// Check for a head step.
		const int step_difference = ((value&3) - (previous_port_b_output_&3))&3;
		if(step_difference && delegate_) {
			delegate_->drive_via_did_step_head(*this, (step_difference == 1) ? 1 : -1);
		}

		// Check for a change in density.
		const int density_difference = (previous_port_b_output_^value) & (3 << 5);
		if(density_difference && delegate_) {
			delegate_->drive_via_did_set_data_density(*this, (value >> 5)&3);
		}

		// Post the LED status.
		if(observer_) {
			observer_->set_led_status("Drive", value&8);
		}

		previous_port_b_output_ = value;
	}
}

template <>
void DriveVIA::set_port_output<MOS::MOS6522::Port::A>(const uint8_t value, uint8_t) {
	if(delegate_) {
		delegate_->drive_via_set_to_shifter_output(*this, value);
	}
}

void DriveVIA::set_activity_observer(Activity::Observer *const observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("Drive");
		observer->set_led_status("Drive", previous_port_b_output_&8);
	}
}

// MARK: - SerialPort.

void SerialPort::set_input(const Serial::Line line, const Serial::LineLevel level) {
	serial_port_via_->set_serial_line_state(line, bool(level), *via_);
}

void SerialPort::connect(SerialPortVIA &serial_port_via, MOS::MOS6522::MOS6522<SerialPortVIA> &via) {
	serial_port_via_ = &serial_port_via;
	via_ = &via;
}
