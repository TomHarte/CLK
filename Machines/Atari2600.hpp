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
#include "../Outputs/CRT.hpp"

namespace Atari2600 {

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(size_t length, const uint8_t *data);

		Outputs::CRT *get_crt() { return _crt; }

	private:
		uint8_t _rom[4096], _ram[128];
		uint16_t _romMask;

		uint64_t _timestamp;

		// the timer
		unsigned int _piaTimerValue;
		unsigned int _piaTimerShift;

		// graphics registers
		uint8_t _playField[3], _playFieldControl;

		// graphics output
		int _horizontalTimer;
		bool _vSyncEnabled, _vBlankEnabled;

		enum OutputState {
			Sync,
			Blank,
			Pixel
		};

		void output_pixels(int count);
		void get_output_pixel(uint8_t *pixel, int offset);
		void output_state(OutputState state, uint8_t *pixel);
		Outputs::CRT *_crt;

		// latched output state
		int _lastOutputStateDuration;
		OutputState _lastOutputState;
		uint8_t *_outputBuffer;
};

}

#endif /* Atari2600_cpp */
