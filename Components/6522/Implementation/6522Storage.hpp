//
//  6522Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef _522Storage_hpp
#define _522Storage_hpp

#include <cstdint>

namespace MOS {
namespace MOS6522 {

class MOS6522Storage {
	protected:
		// Phase toggle
		bool is_phase2_ = false;

		// The registers
		struct Registers {
			// "A low reset (RES) input clears all R6522 internal registers to logic 0"
			uint8_t output[2] = {0, 0};
			uint8_t input[2] = {0, 0};
			uint8_t data_direction[2] = {0, 0};
			uint16_t timer[2] = {0, 0};
			uint16_t timer_latch[2] = {0, 0};
			uint16_t last_timer[2] = {0, 0};
			int next_timer[2] = {-1, -1};
			uint8_t shift = 0;
			uint8_t auxiliary_control = 0;
			uint8_t peripheral_control = 0;
			uint8_t interrupt_flags = 0;
			uint8_t interrupt_enable = 0;

			bool timer_needs_reload = false;
			uint8_t timer_port_b_output = 0xff;
		} registers_;

		// Control state.
		struct {
			bool lines[2] = {false, false};
		} control_inputs_[2];

		enum class LineState {
			On, Off, Input
		};
		struct {
			LineState lines[2] = {LineState::Input, LineState::Input};
		} control_outputs_[2];

		enum class HandshakeMode {
			None,
			Handshake,
			Pulse
		} handshake_modes_[2] = { HandshakeMode::None, HandshakeMode::None };

		bool timer_is_running_[2] = {false, false};
		bool last_posted_interrupt_status_ = false;
		int shift_bits_remaining_ = 8;

		enum InterruptFlag: uint8_t {
			CA2ActiveEdge	= 1 << 0,
			CA1ActiveEdge	= 1 << 1,
			ShiftRegister	= 1 << 2,
			CB2ActiveEdge	= 1 << 3,
			CB1ActiveEdge	= 1 << 4,
			Timer2			= 1 << 5,
			Timer1			= 1 << 6,
		};

		enum class ShiftMode {
			Disabled = 0,
			InUnderT2 = 1,
			InUnderPhase2 = 2,
			InUnderCB1 = 3,
			OutUnderT2FreeRunning = 4,
			OutUnderT2 = 5,
			OutUnderPhase2 = 6,
			OutUnderCB1 = 7
		};
		bool timer1_is_controlling_pb7() const {
			return registers_.auxiliary_control & 0x80;
		}
		bool timer1_is_continuous() const {
			return registers_.auxiliary_control & 0x40;
		}
		bool is_shifting_out() const {
			return registers_.auxiliary_control & 0x10;
		}
		int timer2_clock_decrement() const {
			return 1 ^ ((registers_.auxiliary_control >> 5)&1);
		}
		int timer2_pb6_decrement() const {
			return (registers_.auxiliary_control >> 5)&1;
		}
		ShiftMode shift_mode() const {
			return ShiftMode((registers_.auxiliary_control >> 2) & 7);
		}
		bool portb_is_latched() const {
			return registers_.auxiliary_control & 0x02;
		}
		bool port1_is_latched() const {
			return registers_.auxiliary_control & 0x01;
		}
};

}
}

#endif /* _522Storage_hpp */
