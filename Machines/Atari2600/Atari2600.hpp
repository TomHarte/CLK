//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include <stdint.h>

#include "../../Processors/6502/CPU6502.hpp"
#include "../../Components/6532/6532.hpp"
#include "../CRTMachine.hpp"
#include "Speaker.hpp"

#include "../ConfigurationTarget.hpp"
#include "Atari2600Inputs.h"

namespace Atari2600 {

const unsigned int number_of_upcoming_events = 6;
const unsigned int number_of_recorded_counters = 7;

class PIA: public MOS::MOS6532<PIA> {
	public:
		inline uint8_t get_port_input(int port)
		{
			return port_values_[port];
		}

		inline void update_port_input(int port, uint8_t mask, bool set)
		{
			if(set) port_values_[port] &= ~mask; else port_values_[port] |= mask;
			set_port_did_change(port);
		}

		PIA() :
			port_values_{0xff, 0xff}
		{}

	private:
		uint8_t port_values_[2];

};

class Machine:
	public CPU6502::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {

	public:
		Machine();
		~Machine();

		void configure_as_target(const StaticAnalyser::Target &target);
		void switch_region();

		void set_digital_input(Atari2600DigitalInput input, bool state);
		void set_switch_is_enabled(Atari2600Switch input, bool state);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise();

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _crt; }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return _speaker; }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }
		// TODO: different rate for PAL

	private:
		uint8_t *_rom, *_romPages[4];
		size_t _rom_size;

		// the RIOT
		PIA _mos6532;

		// playfield registers
		uint8_t _playfieldControl;
		uint8_t _playfieldColour;
		uint8_t _backgroundColour;
		uint8_t _playfield[41];

		// ... and derivatives
		int _ballSize, _missileSize[2];

		// delayed clock events
		enum OutputState {
			Sync,
			Blank,
			ColourBurst,
			Pixel
		};

		struct Event {
			enum Action {
				Playfield			= 1 << 0,
				ResetCounter		= 1 << 1,

				HMoveSetup			= 1 << 2,
				HMoveCompare		= 1 << 3,
				HMoveDecrement		= 1 << 4,
			};
			int updates;

			OutputState state;
			uint8_t playfieldPixel;
			int counter;

			Event() : updates(0), playfieldPixel(0) {}
		} _upcomingEvents[number_of_upcoming_events];
		unsigned int _upcomingEventsPointer;

		// object counters
		struct ObjectCounter {
			int count;			// the counter value, multiplied by four, counting phase
			int pixel;			// for non-sprite objects, a count of cycles since the last counter reset; for sprite objects a count of pixels so far elapsed
			int broad_pixel;	// for sprite objects, a count of cycles since the last counter reset; otherwise unused

			ObjectCounter() : count(0), pixel(0), broad_pixel(0) {}
		} _objectCounter[number_of_recorded_counters][5];
		unsigned int _objectCounterPointer;

		// the latched playfield output
		uint8_t _playfieldOutput, _nextPlayfieldOutput;

		// player registers
		uint8_t _playerColour[2];
		uint8_t _playerReflectionMask[2];
		uint8_t _playerGraphics[2][2];
		uint8_t _playerGraphicsSelector[2];
		bool _playerStart[2];

		// object flags
		bool _hasSecondCopy[2];
		bool _hasThirdCopy[2];
		bool _hasFourthCopy[2];
		uint8_t _objectMotion[5];		// the value stored to this counter's motion register

		// player + missile registers
		uint8_t _playerAndMissileSize[2];

		// missile registers
		uint8_t _missileGraphicsEnable[2];
		bool _missileGraphicsReset[2];

		// ball registers
		uint8_t _ballGraphicsEnable[2];
		uint8_t _ballGraphicsSelector;

		// graphics output
		unsigned int _horizontalTimer;
		bool _vSyncEnabled, _vBlankEnabled;

		// horizontal motion control
		uint8_t _hMoveCounter;
		uint8_t _hMoveFlags;

		// joystick state
		uint8_t _tiaInputValue[2];

		// collisions
		uint8_t _collisions[8];

		void output_pixels(unsigned int count);
		uint8_t get_output_pixel();
		void update_timers(int mask);

		// outputs
		std::shared_ptr<Outputs::CRT::CRT> _crt;
		std::shared_ptr<Speaker> _speaker;

		// current mode
		bool _is_pal_region;

		// speaker backlog accumlation counter
		unsigned int _cycles_since_speaker_update;
		void update_audio();

		// latched output state
		unsigned int _lastOutputStateDuration;
		OutputState _stateByExtendTime[2][57];
		OutputState *_stateByTime;
		OutputState _lastOutputState;
		uint8_t *_outputBuffer;

		// lookup table for collision reporting
		uint8_t _reportedCollisions[64][8];
		void setup_reported_collisions();
};

}

#endif /* Atari2600_cpp */
