//
//  MFP68901.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MFP68901_hpp
#define MFP68901_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Motorola {
namespace MFP68901 {

class PortHandler {
	public:
		// TODO: announce changes in output.
};

class MFP68901 {
	public:
		uint8_t read(int address);
		void write(int address, uint8_t value);

		void run_for(HalfCycles);
		HalfCycles get_next_sequence_point();

		void set_timer_event_input(int channel, bool value);

		void set_port_handler(PortHandler *);
		void set_port_input(uint8_t);
		uint8_t get_port_output();

	private:
		// MARK: - Timers
		enum class TimerMode {
			Stopped, EventCount, Delay, PulseWidth
		};
		void set_timer_mode(int timer, TimerMode, int prescale, bool reset_timer);
		void set_timer_data(int timer, uint8_t);
		uint8_t get_timer_data(int timer);
		void decrement_timer(int timer);

		struct Timer {
			TimerMode mode = TimerMode::Stopped;
			uint8_t value = 0;
			uint8_t reload_value = 0;
			int prescale = 1;
			int divisor = 0;
			bool event_input = false;
		} timers_[4];

		HalfCycles cycles_left_;

		// MARK: - GPIP
		uint8_t gpip_input_ = 0;
		uint8_t gpip_output_ = 0;
		uint8_t gpip_active_edge_ = 0;
		uint8_t gpip_direction_ = 0;
		uint8_t gpip_interrupt_state_ = 0;

		void reevaluate_gpip_interrupts();

		// MARK: - Interrupts

		// Ad hoc documentation: there seems to be a four-stage process here.
		// This is my current understanding:
		//
		// Interrupt in-service refers to whether the signal that would cause an
		// interrupt is active.
		//
		// If the interrupt is in-service and enabled, it will be listed as pending.
		//
		// If a pending interrupt is enabled in the interrupt mask, it will generate
		// a processor interrupt.
		//
		// So, the designers seem to have wanted to allow for polling and interrupts,
		// and then also decided to have some interrupts be able to be completely
		// disabled, so that don't even show up for polling.
		uint8_t interrupt_in_service_[2] = {0, 0};
		uint8_t interrupt_enable_[2] = {0, 0};
		uint8_t interrupt_pending_[2] = {0, 0};
		uint8_t interrupt_mask_[2] = {0, 0};

		enum Interrupt {
			GPIP0 = 0,
			GPIP1,
			GPIP2,
			GPIP3,
			TimerD,
			TimerC,
			GPIP4,
			GPIP5,
			TimerB,
			TransmitError,
			TransmitBufferEmpty,
			ReceiveError,
			ReceiveBufferFull,
			GPIP6,
			GPIP7
		};
		void begin_interrupt(Interrupt interrupt);
		void end_interrupt(Interrupt interrupt);
};

}
}

#endif /* MFP68901_hpp */
