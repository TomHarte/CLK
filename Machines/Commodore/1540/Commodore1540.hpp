//
//  Commodore1540.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Commodore1540_hpp
#define Commodore1540_hpp

#include "../../../Processors/6502/CPU6502.hpp"
#include "../../../Components/6522/6522.hpp"
#include "../SerialBus.hpp"

namespace Commodore {
namespace C1540 {

class SerialPortVIA: public MOS::MOS6522<SerialPortVIA>, public MOS::MOS6522IRQDelegate {
	public:
		using MOS6522IRQDelegate::set_interrupt_status;

		SerialPortVIA() : _portB(0x9f) {}

		uint8_t get_port_input(Port port) {
			if(port) {
				return _portB;
			}

			return 0xff;
		}

		void set_port_output(Port port, uint8_t value, uint8_t mask) {
			if(port) {
				std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
				if(serialPort) {
					serialPort->set_output(::Commodore::Serial::Line::Attention, !(value&0x10));
					serialPort->set_output(::Commodore::Serial::Line::Clock, !(value&0x08));
					serialPort->set_output(::Commodore::Serial::Line::Data, !(value&0x02));
				}
//				printf("1540 serial port VIA port B: %02x\n", value);
			}
//			else
//				printf("1540 serial port VIA port A: %02x\n", value);
		}

		void set_serial_line_state(::Commodore::Serial::Line line, bool value) {
//			printf("1540 Serial port line %d: %s\n", line, value ? "on" : "off");
			switch(line) {
				default: break;
				case ::Commodore::Serial::Line::Data:		_portB = (_portB & ~0x01) | (value ? 0x00 : 0x01);		break;
				case ::Commodore::Serial::Line::Clock:		_portB = (_portB & ~0x04) | (value ? 0x00 : 0x04);		break;
				case ::Commodore::Serial::Line::Attention:
					_portB = (_portB & ~0x80) | (value ? 0 : 0x80);
					set_control_line_input(Port::A, Line::One, !value);	// truth here is active low; the 6522 takes true to be high
				break;
			}
		}

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
			_serialPort = serialPort;
		}

	private:
		uint8_t _portB;
		std::weak_ptr<::Commodore::Serial::Port> _serialPort;
};

class DriveVIA: public MOS::MOS6522<DriveVIA>, public MOS::MOS6522IRQDelegate {
	public:
		using MOS6522IRQDelegate::set_interrupt_status;

		uint8_t get_port_input(Port port) {
			return 0xff;
		}
};

class SerialPort : public ::Commodore::Serial::Port {
	public:
		void set_input(::Commodore::Serial::Line line, bool value) {
			std::shared_ptr<SerialPortVIA> serialPortVIA = _serialPortVIA.lock();
			if(serialPortVIA) serialPortVIA->set_serial_line_state(line, value);
		}

		void set_serial_port_via(std::shared_ptr<SerialPortVIA> serialPortVIA) {
			_serialPortVIA = serialPortVIA;
		}

	private:
		std::weak_ptr<SerialPortVIA> _serialPortVIA;
};

class Machine:
	public CPU6502::Processor<Machine>,
	public MOS::MOS6522IRQDelegate::Delegate {

	public:
		Machine();
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(const uint8_t *rom);
		void set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus);

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

	private:
		uint8_t _ram[0x800];
		uint8_t _rom[0x4000];

		std::shared_ptr<SerialPortVIA> _serialPortVIA;
		std::shared_ptr<SerialPort> _serialPort;
		DriveVIA _driveVIA;
};

}
}

#endif /* Commodore1540_hpp */
