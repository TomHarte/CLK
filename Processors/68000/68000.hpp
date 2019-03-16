//
//  68000.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MC68000_h
#define MC68000_h

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../RegisterSizes.hpp"

namespace CPU {
namespace MC68000 {

/*!
	A microcycle is an atomic unit of 68000 bus activity — it is a single item large enough
	fully to specify a sequence of bus events that occur without any possible interruption.

	Concretely, a standard read cycle breaks down into at least two microcycles:

		1) a 5 half-cycle length microcycle in which the address strobe is signalled; and
		2) a 3 half-cycle length microcycle in which at least one of the data strobes is
		signalled, and the data bus is sampled.

	That is, assuming DTack were signalled when microcycle (1) ended. If not then additional
	wait state microcycles would fall between those two parts.

	The 68000 data sheet defines when the address becomes valid during microcycle (1), and
	when the address strobe is actually asserted. But those timings are fixed. So simply
	telling you that this was a microcycle during which the address trobe was signalled is
	sufficient fully to describe the bus activity.

	(Aside: see the 68000 template's definition for options re: implicit DTack; if your
	68000 owner can always predict exactly how long it will hold DTack following observation
	of an address-strobing microcycle, it can just supply those periods for accounting and
	avoid the runtime cost of actual DTack emulation. But such as the bus allows.)
*/
struct Microcycle {
	/*
		The operation code is composed of several parts; a compound low part
		that can be masked off with TypeMask identifies the type of the cycle;
		some of the other status lines are also present in the top parts of the int.
	*/
	static const int TypeMask		= 3;

	/// A NewAddress cycle is one in which the address strobe is initially low but becomes high;
	/// this correlates to states 0 to 5 of a standard read/write cycle.
	static const int Idle			= 0;

	/// A NewAddress cycle is one in which the address strobe is initially low but becomes high;
	/// this correlates to states 0 to 5 of a standard read/write cycle.
	static const int NewAddress		= 1;

	/// A SameAddress cycle is one in which the address strobe is continuously asserted, but neither
	/// of the data strobes are.
	static const int SameAddress	= 2;

	/// Indicates that the address and both data select strobes are active.
	static const int SelectWord		= 1 << 2;

	/// Indicates that the address strobe and exactly one of the data strobes are active; you can determine
	/// which by inspecting the low bit of the provided address. The RW line indicates a read.
	static const int SelectByte		= 1 << 3;

	/// If set, indicates a read. Otherwise, a write.
	static const int Read 			= 1 << 4;

	/// Contains the value of line FC0.
	static const int IsData 		= 1 << 5;

	/// Contains the value of line FC1.
	static const int IsProgram 		= 1 << 6;

	int operation = 0;
	HalfCycles length = HalfCycles(2);

	/*!
		For expediency, this provides a full 32-bit byte-resolution address — e.g.
		if reading indirectly via an address register, this will indicate the full
		value of the address register.

		The receiver should ignore bits 0 and 24+.
	*/
	const uint32_t *address = nullptr;
	RegisterPair16 *value = nullptr;

	// Various inspectors.

	/*! @returns true if any data select line is active; @c false otherwise. */
	inline bool data_select_active() const {
		return bool(operation & (SelectWord | SelectByte));
	}

	/*!
		@returns 0 if this byte access wants the low part of a 16-bit word; 8 if it wants the high part.
	*/
	inline unsigned int byte_shift() const {
		return (((*address) & 1) << 3) ^ 8;
	}

	/*!
		@returns 0x00ff if this byte access wants the low part of a 16-bit word; 0xff00 if it wants the high part.
	*/
	inline unsigned int byte_mask() const {
		return 0xff00 >> (((*address) & 1) << 3);
	}

	inline int lower_data_select() const {
		return (operation & SelectByte) & ((*address & 1) << 3);
	}

	inline int upper_data_select() const {
		return (operation & SelectByte) & ~((*address & 1) << 3);
	}

	uint32_t word_address() const {
		return (address ? (*address) & 0x00fffffe : 0) >> 1;
	}
};

/*!
	This is the prototype for a 68000 bus handler; real bus handlers can descend from this
	in order to get default implementations of any changes that may occur in the expected interface.
*/
class BusHandler {
	public:
		/*!
			Provides the bus handler with a single Microcycle to 'perform'.

			FC0 and FC1 are provided inside the microcycle as the IsData and IsProgram
			flags; FC2 is provided here as is_supervisor — it'll be either 0 or 1.
		*/
		HalfCycles perform_bus_operation(const Microcycle &cycle, int is_supervisor) {
			return HalfCycles(0);
		}

		void flush() {}
};

#include "Implementation/68000Storage.hpp"

class ProcessorBase: public ProcessorStorage {
};

template <class T, bool dtack_is_implicit> class Processor: public ProcessorBase {
	public:
		Processor(T &bus_handler) : ProcessorBase(), bus_handler_(bus_handler) {}

		void run_for(HalfCycles duration);

	private:
		T &bus_handler_;
};

#include "Implementation/68000Implementation.hpp"

}
}

#endif /* MC68000_h */
