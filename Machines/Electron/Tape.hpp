//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_Tape_h
#define Electron_Tape_h

#include "../../Storage/Tape/Tape.hpp"
#include "Interrupts.hpp"

#include <cstdint>

namespace Electron {

class Tape: public Storage::Tape::TapePlayer {
	public:
		Tape();

		void run_for_cycles(unsigned int number_of_cycles);

		uint8_t get_data_register();
		void set_data_register(uint8_t value);
		void set_counter(uint8_t value);

		inline uint8_t get_interrupt_status() { return interrupt_status_; }
		void clear_interrupts(uint8_t interrupts);

		class Delegate {
			public:
				virtual void tape_did_change_interrupt_status(Tape *tape) = 0;
		};
		inline void set_delegate(Delegate *delegate) { delegate_ = delegate; }

		inline void set_is_running(bool is_running) { is_running_ = is_running; }
		inline void set_is_enabled(bool is_enabled) { is_enabled_ = is_enabled; }
		void set_is_in_input_mode(bool is_in_input_mode);

	private:
		void process_input_pulse(Storage::Tape::Tape::Pulse pulse);
		inline void push_tape_bit(uint16_t bit);
		inline void get_next_tape_pulse();

		struct {
			int minimum_bits_until_full;
		} input_;
		struct {
			unsigned int cycles_into_pulse;
			unsigned int bits_remaining_until_empty;
		} output_;

		bool is_running_;
		bool is_enabled_;
		bool is_in_input_mode_;

		inline void evaluate_interrupts();
		uint16_t data_register_;

		uint8_t interrupt_status_, last_posted_interrupt_status_;
		Delegate *delegate_;

		enum {
			Long, Short, Unrecognised, Recognised
		} crossings_[4];
};

}

#endif /* Electron_Tape_h */
