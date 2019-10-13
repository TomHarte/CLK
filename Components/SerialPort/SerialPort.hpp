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

		/// Eliminates all future write states, leaving the output at whatever it is now.
		void reset_writing();

		/// Applies all pending write changes instantly.
		void flush_writing();

		/// @returns The instantaneous level of this line.
		bool read();

	private:
		struct Event {
			enum Type {
				Delay, SetHigh, SetLow
			} type;
			int delay;
		};
		std::vector<Event> events_;
		int remaining_delays_ = 0;
		bool level_ = false;
};

/*!
	Defines an RS-232-esque srial port.
*/
class Port {
	public:
};

}

#endif /* SerialPort_hpp */
