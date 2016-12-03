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
#include "../CRTMachine.hpp"
#include "PIA.hpp"
#include "Speaker.hpp"

#include "../ConfigurationTarget.hpp"
#include "Atari2600Inputs.h"

namespace Atari2600 {

const unsigned int number_of_upcoming_events = 6;
const unsigned int number_of_recorded_counters = 7;

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
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return crt_; }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return speaker_; }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }
		// TODO: different rate for PAL

	private:
		uint8_t *rom_, *rom_pages_[4];
		size_t rom_size_;

		// the RIOT
		PIA mos6532_;

		// playfield registers
		uint8_t playfield_control_;
		uint8_t playfield_colour_;
		uint8_t background_colour_;
		uint8_t playfield_[41];

		// ... and derivatives
		int ball_size_, missile_size_[2];

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
			uint8_t playfield_pixel;
			int counter;

			Event() : updates(0), playfield_pixel(0) {}
		} upcoming_events_[number_of_upcoming_events];
		unsigned int upcoming_events_pointer_;

		// object counters
		struct ObjectCounter {
			int count;			// the counter value, multiplied by four, counting phase
			int pixel;			// for non-sprite objects, a count of cycles since the last counter reset; for sprite objects a count of pixels so far elapsed
			int broad_pixel;	// for sprite objects, a count of cycles since the last counter reset; otherwise unused

			ObjectCounter() : count(0), pixel(0), broad_pixel(0) {}
		} object_counter_[number_of_recorded_counters][5];
		unsigned int object_counter_pointer_;

		// the latched playfield output
		uint8_t playfield_output_, next_playfield_output_;

		// player registers
		uint8_t player_colour_[2];
		uint8_t player_reflection_mask_[2];
		uint8_t player_graphics_[2][2];
		uint8_t player_graphics_selector_[2];

		// object flags
		bool has_second_copy_[2];
		bool has_third_copy_[2];
		bool has_fourth_copy_[2];
		uint8_t object_motions_[5];		// the value stored to this counter's motion register

		// player + missile registers
		uint8_t player_and_missile_size_[2];

		// missile registers
		uint8_t missile_graphics_enable_[2];
		bool missile_graphics_reset_[2];

		// ball registers
		uint8_t ball_graphics_enable_[2];
		uint8_t ball_graphics_selector_;

		// graphics output
		unsigned int horizontal_timer_;
		bool vsync_enabled_, vblank_enabled_;

		// horizontal motion control
		uint8_t hmove_counter_;
		uint8_t hmove_flags_;

		// joystick state
		uint8_t tia_input_value_[2];

		// collisions
		uint8_t collisions_[8];

		void output_pixels(unsigned int count);
		uint8_t get_output_pixel();
		void update_timers(int mask);

		// outputs
		std::shared_ptr<Outputs::CRT::CRT> crt_;
		std::shared_ptr<Speaker> speaker_;

		// current mode
		bool is_pal_region_;

		// speaker backlog accumlation counter
		unsigned int cycles_since_speaker_update_;
		void update_audio();

		// latched output state
		unsigned int last_output_state_duration_;
		OutputState state_by_extend_time_[2][57];
		OutputState *state_by_time_;
		OutputState last_output_state_;
		uint8_t *output_buffer_;

		// lookup table for collision reporting
		uint8_t reported_collisions_[64][8];
		void setup_reported_collisions();
};

}

#endif /* Atari2600_cpp */
