//
//  SerialPort.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SerialPort_hpp
#define SerialPort_hpp

#include <vector>
#include "../../Storage/Storage.hpp"

namespace Serial {

/*!
	@c Line connects a single reader and a single writer, allowing timestamped events to be
	published and consumed, potentially with a clock conversion in between. It allows line
	levels to be written and read in larger collections.

	It is assumed that the owner of the reader and writer will ensure that the reader will never
	get ahead of the writer. If the writer posts events behind the reader they will simply be
	given instanteous effect.
*/
class Line {
	public:
		void set_writer_clock_rate(int clock_rate);

		/// Advances the read position by @c cycles relative to the writer's
		/// clock rate.
		void advance_writer(int cycles);

		/// Sets the line to @c level.
		void write(bool level);

		/// Enqueues @c count level changes, the first occurring immediately
		/// after the final event currently posted and each subsequent event
		/// occurring @c cycles after the previous. An additional gap of @c cycles
		/// is scheduled after the final output. The levels to output are
		/// taken from @c levels which is read from lsb to msb. @c cycles is
		/// relative to the writer's clock rate.
		void write(int cycles, int count, int levels);

		/// @returns the number of cycles until currently enqueued write data is exhausted.
		int write_data_time_remaining();

		/// @returns the number of cycles left until it is guaranteed that a passive reader
		/// has received all currently-enqueued bits.
		int transmission_data_time_remaining();

		/// Eliminates all future write states, leaving the output at whatever it is now.
		void reset_writing();

		/// @returns The instantaneous level of this line.
		bool read();

		struct ReadDelegate {
			virtual bool serial_line_did_produce_bit(Line *line, int bit) = 0;
		};
		/*!
			Sets a read delegate, which will receive samples of the output level every
			@c bit_lengths of a second apart subject to a state machine:

				* initially no bits will be delivered;
				* when a zero level is first detected, the line will wait half a bit's length, then start
				sampling at single-bit intervals, passing each bit to the delegate while it returns @c true;
				* as soon as the delegate returns @c false, the line will return to the initial state.
		*/
		void set_read_delegate(ReadDelegate *delegate, Storage::Time bit_length);

	private:
		struct Event {
			enum Type {
				Delay, SetHigh, SetLow
			} type;
			int delay;
		};
		std::vector<Event> events_;
		int remaining_delays_ = 0;
		int transmission_extra_ = 0;
		bool level_ = true;
		int clock_rate_ = 0;

		ReadDelegate *read_delegate_ = nullptr;
		Storage::Time read_delegate_bit_length_, time_left_in_bit_;
		int write_cycles_since_delegate_call_ = 0;
		enum class ReadDelegatePhase {
			WaitingForZero,
			Serialising
		} read_delegate_phase_ = ReadDelegatePhase::WaitingForZero;

		void update_delegate(bool level);
		int minimum_write_cycles_for_read_delegate_bit();
};

/*!
	Defines an RS-232-esque srial port.
*/
class Port {
	public:
};

}

#endif /* SerialPort_hpp */
