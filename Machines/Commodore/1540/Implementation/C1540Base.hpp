//
//  C1540Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Mk2/6502Mk2.hpp"
#include "Components/6522/6522.hpp"

#include "Machines/Commodore/SerialBus.hpp"

#include "Activity/Source.hpp"
#include "Storage/Disk/Disk.hpp"

#include "Storage/Disk/Controller/DiskController.hpp"

#include "Machines/Commodore/1540/C1540.hpp"

namespace Commodore::C1540 {

/*!
	An implementation of the serial-port VIA in a Commodore 1540: the VIA that facilitates all
	IEC bus communications.

	It is wired up such that Port B contains:
		Bit 0:		data input; 1 if the line is low, 0 if it is high;
		Bit 1:		data output; 1 if the line should be low, 0 if it should be high;
		Bit 2:		clock input; 1 if the line is low, 0 if it is high;
		Bit 3:		clock output; 1 if the line is low, 0 if it is high;
		Bit 4:		attention acknowledge output; exclusive ORd with the attention input and ORd onto the data output;
		Bits 5/6:	device select input; the 1540 will act as device 8 + [value of bits]
		Bit 7:		attention input; 1 if the line is low, 0 if it is high

	The attention input is also connected to CA1, similarly invertedl; the CA1 wire will be high when the bus is low and vice versa.
*/
class SerialPortVIA: public MOS::MOS6522::IRQDelegatePortHandler {
public:
	template <MOS::MOS6522::Port>
	uint8_t get_port_input() const;

	template <MOS::MOS6522::Port>
	void set_port_output(uint8_t value, uint8_t mask);

	void set_serial_line_state(Commodore::Serial::Line, bool, MOS::MOS6522::MOS6522<SerialPortVIA> &);

	void set_serial_port(Commodore::Serial::Port &);

private:
	uint8_t port_b_ = 0x0;
	Commodore::Serial::Port *serial_port_ = nullptr;
	bool attention_acknowledge_level_ = false;
	bool attention_level_input_ = true;
	bool data_level_output_ = false;

	void update_data_line();
};

/*!
	An implementation of the drive VIA in a Commodore 1540: the VIA that is used to interface with the disk.

	It is wired up such that Port B contains:
		Bits 0/1:	head step direction
		Bit 2:		motor control
		Bit 3:		LED control (TODO)
		Bit 4:		write protect photocell status (TODO)
		Bits 5/6:	read/write density
		Bit 7:		0 if sync marks are currently being detected, 1 otherwise.

	... and Port A contains the byte most recently read from the disk or the byte next to write to the disk, depending on data direction.

	It is implied that CA2 might be used to set processor overflow, CA1 a strobe for data input, and one of the CBs being definitive on
	whether the disk head is being told to read or write, but it's unclear and I've yet to investigate. So, TODO.
*/
class DriveVIA: public MOS::MOS6522::IRQDelegatePortHandler {
public:
	struct Delegate {
		virtual void drive_via_did_step_head(DriveVIA &, int direction) = 0;
		virtual void drive_via_did_set_data_density(DriveVIA &, int density) = 0;
	};
	void set_delegate(Delegate *);

	template <MOS::MOS6522::Port>
	uint8_t get_port_input() const;

	void set_sync_detected(bool);
	void set_data_input(uint8_t);
	bool should_set_overflow();
	bool motor_enabled();

	template <MOS::MOS6522::Port, MOS::MOS6522::Line>
	void set_control_line_output(bool value);

	template <MOS::MOS6522::Port>
	void set_port_output(uint8_t value, uint8_t direction_mask);

	void set_activity_observer(Activity::Observer *);

private:
	uint8_t port_b_ = 0xff, port_a_ = 0xff;
	bool should_set_overflow_ = false;
	bool drive_motor_ = false;
	uint8_t previous_port_b_output_ = 0;
	Delegate *delegate_ = nullptr;
	Activity::Observer *observer_ = nullptr;
};

/*!
	An implementation of the C1540's serial port; this connects incoming line levels to the serial-port VIA.
*/
class SerialPort : public ::Commodore::Serial::Port {
public:
	void set_input(Commodore::Serial::Line, Commodore::Serial::LineLevel) override;
	void connect(SerialPortVIA &, MOS::MOS6522::MOS6522<SerialPortVIA>&);

private:
	SerialPortVIA *serial_port_via_ = nullptr;
	MOS::MOS6522::MOS6522<SerialPortVIA> *via_ = nullptr;
};

class MachineBase:
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public DriveVIA::Delegate,
	public Storage::Disk::Controller {

public:
	MachineBase(Personality, const ROM::Map &);

	/// Attaches the activity observer to this C1540.
	void set_activity_observer(Activity::Observer *);

	// to satisfy CPU::MOS6502::Processor
	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT, CPU::MOS6502Mk2::data_t<operation>);

protected:
	// to satisfy MOS::MOS6522::Delegate
	void mos6522_did_change_interrupt_status(void *mos6522) override;

	// to satisfy DriveVIA::Delegate
	void drive_via_did_step_head(DriveVIA &, int direction) override;
	void drive_via_did_set_data_density(DriveVIA &, int density) override;

	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::AnyCycle;
		using BusHandlerT = MachineBase;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::M6502, M6502Traits> m6502_;

	uint8_t ram_[0x800];
	uint8_t rom_[0x4000];

	SerialPortVIA serial_port_VIA_port_handler_;
	SerialPort serial_port_;
	DriveVIA drive_VIA_port_handler_;

	MOS::MOS6522::MOS6522<DriveVIA> drive_VIA_;
	MOS::MOS6522::MOS6522<SerialPortVIA> serial_port_VIA_;

	int shift_register_ = 0, bit_window_offset_;
	void process_input_bit(int value) override;
	void process_index_hole() override;
};

}
