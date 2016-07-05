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
};

class SerialPort : public ::Commodore::Serial::Port {
	public:
		void set_input(::Commodore::Serial::Line line, bool value) {
		}

		void set_serial_port_via(std::shared_ptr<SerialPortVIA> serialPortVIA) {
			_serialPortVIA = serialPortVIA;
		}

	private:
		std::weak_ptr<SerialPortVIA> _serialPortVIA;
};

class Machine:
	public CPU6502::Processor<Machine> {

	public:
		Machine();
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(const uint8_t *rom);
		void set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus);

	private:
		uint8_t _ram[0x800];
		uint8_t _rom[0x4000];

		std::shared_ptr<SerialPortVIA> _serialPortVIA;
		std::shared_ptr<SerialPort> _serialPort;
};

}
}

#endif /* Commodore1540_hpp */
