//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include "../Processors/6502/CPU6502.hpp"
#include "../Outputs/CRT.hpp"
#include <stdint.h>

namespace Atari2600 {

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();
		~Machine();

		int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(size_t length, const uint8_t *data);
		void switch_region();

		Outputs::CRT *get_crt() { return _crt; }

	private:
		uint8_t *_rom, *_romPages[4], _ram[128];
		size_t _rom_size;

		uint64_t _timestamp;

		// the timer
		unsigned int _piaTimerValue;
		unsigned int _piaTimerShift, _writtenPiaTimerShift;
		uint8_t _piaTimerStatus;

		// playfield registers
		uint8_t _playfield[3], _playfieldControl;
		uint8_t _playfieldColour;
		uint8_t _backgroundColour;

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
		int32_t _horizontalTimer;
		bool _vSyncEnabled, _vBlankEnabled;
		bool _vBlankExtend;
		uint8_t _hMoveCounter, _hMoveFlags;
		uint8_t _objectCounter[5], _objectMotion[5];

		// collisions
		uint8_t _collisions[8];

		enum OutputState {
			Sync,
			Blank,
			Pixel
		};

		void output_pixels(int count);
		void get_output_pixel(uint8_t *pixel, int offset);
		Outputs::CRT *_crt;

		// latched output state
		int _lastOutputStateDuration;
		OutputState _lastOutputState;
		uint8_t *_outputBuffer;
};

}

#endif /* Atari2600_cpp */
