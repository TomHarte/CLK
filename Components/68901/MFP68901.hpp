//
//  MFP68901.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"

#include <cstdint>

namespace Motorola::MFP68901 {

class PortHandler {
	public:
		// TODO: announce changes in output.
};

/*!
	Models the Motorola 68901 Multi-Function Peripheral ('MFP').
*/
class MFP68901: public ClockingHint::Source {
	public:
		/// @returns the result of a read from @c address.
		uint8_t read(int address);

		/// Performs a write of @c value to @c address.
		void write(int address, uint8_t value);

		/// Advances the MFP by the supplied number of HalfCycles.
		void run_for(HalfCycles);

		/// @returns the number of cycles until the next possible sequence point — the next time
		/// at which the interrupt line _might_ change. This object conforms to ClockingHint::Source
		/// so that mechanism can also be used to reduce the quantity of calls into this class.
		///
		/// @discussion TODO, alas.
		HalfCycles next_sequence_point();

		/// Sets the current level of either of the timer event inputs — TAI and TBI in datasheet terms.
		template <int channel>
		void set_timer_event_input(bool value);

		/// Sets a port handler, a receiver that will be notified upon any change in GPIP output.
		///
		/// @discussion TODO.
		void set_port_handler(PortHandler *);

		/// Sets the current input GPIP values.
		void set_port_input(uint8_t);

		/// @returns the current GPIP output values.
		///
		/// @discussion TODO.
		uint8_t get_port_output();

		/// @returns @c true if the interrupt output is currently active; @c false otherwise.s
		bool get_interrupt_line();

		static constexpr int NoAcknowledgement = 0x100;

		/// Communicates an interrupt acknowledge cycle.
		///
		/// @returns the vector placed on the bus if any; @c NoAcknowledgement if nothing is loaded.
		int acknowledge_interrupt();

		struct InterruptDelegate {
			/// Informs the delegate of a change in the interrupt line of the nominated MFP.
			virtual void mfp68901_did_change_interrupt_status(MFP68901 *) = 0;
		};
		/// Sets a delegate that will be notified upon any change in the interrupt line.
		void set_interrupt_delegate(InterruptDelegate *delegate);

		// ClockingHint::Source.
		ClockingHint::Preference preferred_clocking() const final;

	private:
		// MARK: - Timers
		enum class TimerMode {
			Stopped, EventCount, Delay, PulseWidth
		};
		void set_timer_mode(int timer, TimerMode, int prescale, bool reset_timer);
		void set_timer_data(int timer, uint8_t);
		uint8_t get_timer_data(int timer);
		template <int timer> void decrement_timer(int amount);
		template <int timer> void run_timer_for(int cycles);

		struct Timer {
			TimerMode mode = TimerMode::Stopped;
			uint8_t value = 0;
			uint8_t reload_value = 0;
			int prescale = 1;
			int prescale_count = 1;
			bool event_input = false;
		} timers_[4];
		uint8_t timer_ab_control_[2] = { 0, 0 };
		uint8_t timer_cd_control_ = 0;

		HalfCycles cycles_left_;

		// MARK: - GPIP
		uint8_t gpip_input_ = 0;
		uint8_t gpip_output_ = 0;
		uint8_t gpip_active_edge_ = 0;
		uint8_t gpip_direction_ = 0;
		uint8_t gpip_interrupt_state_ = 0;

		void reevaluate_gpip_interrupts();

		// MARK: - Interrupts

		InterruptDelegate *interrupt_delegate_ = nullptr;

		// Ad hoc documentation:
		//
		// An interrupt becomes pending if it is enabled at the time it occurs.
		//
		// If a pending interrupt is enabled in the interrupt mask, a processor
		// interrupt is generated. Otherwise no processor interrupt is generated.
		//
		// (Disabling a bit in the enabled mask also instantaneously clears anything
		// in the pending mask.)
		//
		// The user can write to the pending interrupt register; a write
		// masks whatever is there — so you can disable bits but you cannot set them.
		//
		// If the vector register's 'S' bit is set then software end-of-interrupt mode applies:
		// Acknowledgement of an interrupt clears that interrupt's pending bit, but also sets
		// its in-service bit. That bit will remain set until the user writes a zero to its position.
		// If any bits are set in the in-service register, then they will prevent lower-priority
		// interrupts from being signalled to the CPU. Further interrupts of the same or a higher
		// priority may occur.
		//
		// If the vector register's 'S' bit is clear then automatic end-of-interrupt mode applies:
		// Acknowledgement of an interrupt will automatically clear the corresponding
		// pending bit.
		//
		int interrupt_enable_ = 0;
		int interrupt_pending_ = 0;
		int interrupt_mask_ = 0;
		int interrupt_in_service_ = 0;
		bool interrupt_line_ = false;
		uint8_t interrupt_vector_ = 0;

		enum Interrupt {
			GPIP0				= (1 << 0),
			GPIP1				= (1 << 1),
			GPIP2				= (1 << 2),
			GPIP3				= (1 << 3),
			TimerD				= (1 << 4),
			TimerC				= (1 << 5),
			GPIP4				= (1 << 6),
			GPIP5				= (1 << 7),

			TimerB				= (1 << 8),
			TransmitError		= (1 << 9),
			TransmitBufferEmpty	= (1 << 10),
			ReceiveError		= (1 << 11),
			ReceiveBufferFull	= (1 << 12),
			TimerA				= (1 << 13),
			GPIP6				= (1 << 14),
			GPIP7				= (1 << 15),
		};
		void begin_interrupts(int interrupt);
		void end_interrupts(int interrupt);
		void update_interrupts();

		/// @returns the most significant bit set in v, assuming it is one of the least significant 16.
		inline static int msb16(int v) {
			// Saturate all bits below the MSB.
			v |= v >> 1;
			v |= v >> 2;
			v |= v >> 4;
			v |= v >> 8;

			// Throw away lesser bits.
			return (v+1) >> 1;
		}
};

}
