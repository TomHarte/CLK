//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../../Outputs/CRT.hpp"
#include <stdint.h>
#include "Atari2600Inputs.h"

namespace Electron {

enum ROMSlot: uint8_t {
	ROMSlot0 = 0,
	ROMSlot1,	ROMSlot2,	ROMSlot3,
	ROMSlot4,	ROMSlot5,	ROMSlot6,	ROMSlot7,

	ROMSlotKeyboard = 8,	ROMSlot9,
	ROMSlotBASIC = 10,		ROMSlot11,

	ROMSlot12,	ROMSlot13,	ROMSlot14,	ROMSlot15,

	ROMSlotOS
};

enum Interrupt: uint8_t {
	InterruptRealTimeClock		= 0x01,
	InterruptDisplayEnd			= 0x02,
	InterruptTransmitDataEmpty	= 0x04,
	InterruptReceiveDataFull	= 0x08,
	InterruptHighToneDetect		= 0x10
};

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();
		~Machine();

		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);

		Outputs::CRT *get_crt() { return _crt; }

	private:
		uint8_t _os[16384], _basic[16384], _ram[32768];
		uint8_t _interruptStatus, _interruptControl;
		ROMSlot _activeRom;

		Outputs::CRT *_crt;

		int _frameCycles, _outputPosition;
		uint16_t _startScreenAddress, _currentScreenAddress;

		inline void update_display();
		inline void signal_interrupt(Interrupt interrupt);
		inline void evaluate_interrupts();
};

}

#endif /* Electron_hpp */
