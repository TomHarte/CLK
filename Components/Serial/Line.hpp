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
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"

namespace Serial {

/*!
	Models one of two connections, either:

		(i) a plain single-line serial; or
		(ii) a two-line data + clock.

	In both cases connects a single reader to a single writer.

	When operating as a single-line serial connection:

		Provides a mechanism for the writer to enqueue levels arbitrarily far
		ahead of the current time, which are played back only as the
		write queue advances. Permits the reader and writer to work at
		different clock rates, and provides a virtual delegate protocol with
		start bit detection.

		Can alternatively be used by reader and/or writer only in immediate
		mode, getting or setting the current level now, without the actor on
		the other end having to have made the same decision.

	When operating as a two-line connection:

		Implies a clock over enqueued data and provides the reader with
		all enqueued bits at appropriate times.
*/
template <bool include_clock> class Line {
	public:
		/// Sets the line to @c level instantaneously.
		void write(bool level);

		/// @returns The instantaneous level of this line.
		bool read() const;

		/// Sets the denominator for the between levels for any data enqueued
		/// via an @c write.
		void set_writer_clock_rate(HalfCycles clock_rate);

		/// Enqueues @c count level changes, the first occurring immediately
		/// after the final event currently posted and each subsequent event
		/// occurring @c cycles after the previous. An additional gap of @c cycles
		/// is scheduled after the final output. The levels to output are
		/// taken from @c levels which is read from lsb to msb. @c cycles is
		/// relative to the writer's clock rate.
		void write(HalfCycles cycles, int count, int levels);

		/// Enqueus every bit from @c value as per the rules of write(HalfCycles, int, int),
		/// either in LSB or MSB order as per the @c lsb_first template flag.
		template <bool lsb_first, typename IntT> void write(HalfCycles cycles, IntT value);

		/// @returns the number of cycles until currently enqueued write data is exhausted.
		forceinline HalfCycles write_data_time_remaining() const {
			return HalfCycles(remaining_delays_);
		}

		/// @returns the number of cycles left until it is guaranteed that a passive reader
		/// has received all currently-enqueued bits.
		forceinline HalfCycles transmission_data_time_remaining() const {
			return HalfCycles(remaining_delays_ + transmission_extra_);
		}

		/// Advances the read position by @c cycles relative to the writer's
		/// clock rate.
		void advance_writer(HalfCycles cycles);

		/// Eliminates all future write states, leaving the output at whatever it is now.
		void reset_writing();

		struct ReadDelegate {
			virtual bool serial_line_did_produce_bit(Line *line, int bit) = 0;
		};
		/*!
			Sets a read delegate.

			Single line serial connection:

				The delegate will receive samples of the output level every
				@c bit_lengths of a second apart subject to a state machine:

					* initially no bits will be delivered;
					* when a zero level is first detected, the line will wait half a bit's length, then start
					sampling at single-bit intervals, passing each bit to the delegate while it returns @c true;
					* as soon as the delegate returns @c false, the line will return to the initial state.

			Two-line clock + data connection:

				The delegate will receive every bit that has been enqueued, spaced as nominated
				by the writer. @c bit_length is ignored, as is the return result of
				@c ReadDelegate::serial_line_did_produce_bit.
		*/
		void set_read_delegate(ReadDelegate *delegate, Storage::Time bit_length = Storage::Time());

	private:
		struct Event {
			enum Type {
				Delay, SetHigh, SetLow
			} type;
			int delay;
		};
		std::vector<Event> events_;
		HalfCycles::IntType remaining_delays_ = 0;
		HalfCycles::IntType transmission_extra_ = 0;
		bool level_ = true;
		HalfCycles clock_rate_ = 0;

		ReadDelegate *read_delegate_ = nullptr;
		Storage::Time read_delegate_bit_length_, time_left_in_bit_;
		int write_cycles_since_delegate_call_ = 0;
		enum class ReadDelegatePhase {
			WaitingForZero,
			Serialising
		} read_delegate_phase_ = ReadDelegatePhase::WaitingForZero;

		void update_delegate(bool level);
		HalfCycles::IntType minimum_write_cycles_for_read_delegate_bit();

		template <bool lsb_first, typename IntT> void
			write_internal(HalfCycles, int, IntT);
};

/*!
	Defines an RS-232-esque srial port.
*/
class Port {
	public:
};

}

#endif /* SerialPort_hpp */
