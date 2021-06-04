//
//  C1540Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef C1540Base_hpp
#define C1540Base_hpp

#include "../../../../Processors/6502/6502.hpp"
#include "../../../../Components/6522/6522.hpp"

#include "../../SerialBus.hpp"

#include "../../../../Activity/Source.hpp"
#include "../../../../Storage/Disk/Disk.hpp"

#include "../../../../Storage/Disk/Controller/DiskController.hpp"

#include "../C1540.hpp"

namespace Commodore {
namespace C1540 {

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
		SerialPortVIA(MOS::MOS6522::MOS6522<SerialPortVIA> &via);

		uint8_t get_port_input(MOS::MOS6522::Port);

		void set_port_output(MOS::MOS6522::Port, uint8_t value, uint8_t mask);
		void set_serial_line_state(::Commodore::Serial::Line, bool);

		void set_serial_port(const std::shared_ptr<::Commodore::Serial::Port> &);

	private:
		MOS::MOS6522::MOS6522<SerialPortVIA> &via_;
		uint8_t port_b_ = 0x0;
		std::weak_ptr<::Commodore::Serial::Port> serial_port_;
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
		class Delegate {
			public:
				virtual void drive_via_did_step_head(void *driveVIA, int direction) = 0;
				virtual void drive_via_did_set_data_density(void *driveVIA, int density) = 0;
		};
		void set_delegate(Delegate *);

		uint8_t get_port_input(MOS::MOS6522::Port port);

		void set_sync_detected(bool);
		void set_data_input(uint8_t);
		bool get_should_set_overflow();
		bool get_motor_enabled();

		void set_control_line_output(MOS::MOS6522::Port, MOS::MOS6522::Line, bool value);

		void set_port_output(MOS::MOS6522::Port, uint8_t value, uint8_t direction_mask);

		void set_activity_observer(Activity::Observer *observer);

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
		void set_input(::Commodore::Serial::Line, ::Commodore::Serial::LineLevel);
		void set_serial_port_via(const std::shared_ptr<SerialPortVIA> &);

	private:
		std::weak_ptr<SerialPortVIA> serial_port_VIA_;
};

class MachineBase:
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public DriveVIA::Delegate,
	public Storage::Disk::Controller {

	public:
		MachineBase(Personality personality, const ROM::Map &roms);

		// to satisfy CPU::MOS6502::Processor
		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value);

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

		// to satisfy DriveVIA::Delegate
		void drive_via_did_step_head(void *driveVIA, int direction);
		void drive_via_did_set_data_density(void *driveVIA, int density);

		/// Attaches the activity observer to this C1540.
		void set_activity_observer(Activity::Observer *observer);

	protected:
		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, MachineBase, false> m6502_;

		uint8_t ram_[0x800];
		uint8_t rom_[0x4000];

		std::shared_ptr<SerialPortVIA> serial_port_VIA_port_handler_;
		std::shared_ptr<SerialPort> serial_port_;
		DriveVIA drive_VIA_port_handler_;

		MOS::MOS6522::MOS6522<DriveVIA> drive_VIA_;
		MOS::MOS6522::MOS6522<SerialPortVIA> serial_port_VIA_;

		int shift_register_ = 0, bit_window_offset_;
		virtual void process_input_bit(int value);
		virtual void process_index_hole();
};

}
}

#endif /* C1540Base_hpp */
