//
//  SerialPort.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SerialPort_hpp
#define SerialPort_hpp

namespace Serial {

/// Signal is an amalgamation of the RS-232-esque signals and those available on the Macintosh
/// and therefore often associated with RS-422.
enum class Signal {
	Receive,
	Transmit,
	ClearToSend,
	RequestToSend,
	DataCarrierDetect,
	OutputHandshake,
	InputHandshake
};

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
		void connect_reader(int clock_rate);
		void disconnect_reader();

		void connect_writer(int clock_rate);
		void disconnect_writer();

		/// Sets the line to @c level after @c cycles relative to the writer's
		/// clock rate have elapsed from the final event currently posted.
		void write(int cycles, bool level);

		/// Enqueues @c count level changes, the first occurring @c cycles
		/// after the final event currently posted and each subsequent event
		/// occurring @c cycles after the previous. The levels to output are
		/// taken from @c levels which is read from lsb to msb. @c cycles is
		/// relative to the writer's clock rate.
		void write(int cycles, int count, int levels);

		/// Advances the read position by @c cycles relative to the reader's
		/// clock rate.
		void advance_reader(int cycles);

		/// @returns The instantaneous level of this line at the current read position.
		bool read();
};

/*!
	Defines an RS-232-esque srial port.
*/
class Port {
	public:
};

}

#endif /* SerialPort_hpp */
