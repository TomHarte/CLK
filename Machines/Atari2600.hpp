//
//  Atari2600.hpp
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include "../Processors/6502/CPU6502.hpp"

namespace Atari2600 {

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(size_t length, const uint8_t *data);

	private:
		uint8_t _rom[4096], _ram[128];
		uint16_t _romMask;
		uint8_t _playField[3], _playFieldControl;

		uint64_t _timestamp;
		bool _vsync, _vblank;

		unsigned int _piaTimerValue;
		unsigned int _piaTimerShift;

		int _horizontalTimer;

		int _pixelPosition;
		uint8_t _playFieldPixel;
		void output_pixels(int count);
};

}

#endif /* Atari2600_cpp */
