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
	InterruptDisplayEnd			= 0x04,
	InterruptRealTimeClock		= 0x08,
	InterruptTransmitDataEmpty	= 0x10,
	InterruptReceiveDataFull	= 0x20,
	InterruptHighToneDetect		= 0x40
};

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();
		~Machine();

		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);

		Outputs::CRT *get_crt() { return _crt; }
		const char *get_signal_decoder();

	private:
		uint8_t _os[16384], _basic[16384], _ram[32768];
		uint8_t _interruptStatus, _interruptControl;
		uint8_t palette[16];
		ROMSlot _activeRom;

		Outputs::CRT *_crt;

		int _frameCycles, _outputPosition;
		uint16_t _startScreenAddress, _startLineAddress, _currentScreenAddress;
		uint8_t *_currentLine;

		inline void update_display();
		inline void signal_interrupt(Interrupt interrupt);
		inline void evaluate_interrupts();
};

}

#endif /* Electron_hpp */
