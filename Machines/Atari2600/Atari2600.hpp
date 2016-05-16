//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../../Outputs/CRT/CRT.hpp"
#include <stdint.h>
#include "Atari2600Inputs.h"

namespace Atari2600 {

class Machine: public CPU6502::Processor<Machine> {

	public:
		Machine();
		~Machine();

		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(size_t length, const uint8_t *data);
		void switch_region();

		void set_digital_input(Atari2600DigitalInput input, bool state);

		Outputs::CRT::CRT *get_crt() { return _crt; }
		void setup_output(float aspect_ratio);
		void close_output();

	private:
		uint8_t *_rom, *_romPages[4], _ram[128];
		size_t _rom_size;

		// the timer
		unsigned int _piaTimerValue;
		unsigned int _piaTimerShift, _writtenPiaTimerShift;
		uint8_t _piaTimerStatus;

		// playfield registers
		uint8_t _playfieldControl;
		uint8_t _playfieldColour;
		uint8_t _backgroundColour;
		uint8_t _playfield[40];

		// player registers
		uint8_t _playerColour[2];
		uint8_t _playerReflection[2];
		uint8_t _playerGraphicsLatch[2], _playerGraphics[2];
		uint8_t _playerGraphicsLatchEnable[2];
		bool _playerStart[2];

		// player + missile registers
		uint8_t _playerAndMissileSize[2];

		// missile registers
		uint8_t _missileGraphicsEnable[2], _missileGraphicsReset[2];

		// ball registers
		uint8_t _ballGraphicsEnable, _ballGraphicsEnableLatch;
		uint8_t _ballGraphicsEnableDelay;

		// graphics output
		unsigned int _horizontalTimer;
		bool _vSyncEnabled, _vBlankEnabled;
		bool _vBlankExtend;
		uint8_t _hMoveCounter;
		bool _hMoveIsCounting, _hMoveWillCount;
		uint8_t _objectCounter[5], _objectMotion[5];
		uint8_t _hMoveFlags;

		// joystick state
		uint8_t _piaDataDirection[2];
		uint8_t _piaDataValue[2];
		uint8_t _tiaInputValue[2];

		// collisions
		uint8_t _collisions[8];

		enum OutputState {
			Sync,
			Blank,
			ColourBurst,
			Pixel
		};

		void output_pixels(unsigned int count);
		uint8_t get_output_pixel();
		Outputs::CRT::CRT *_crt;

		// latched output state
		unsigned int _lastOutputStateDuration;
		OutputState _lastOutputState;
		uint8_t *_outputBuffer;
};

}

#endif /* Atari2600_cpp */
