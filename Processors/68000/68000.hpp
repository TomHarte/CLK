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
#include <vector>

#include "../../ClockReceiver/ClockReceiver.hpp"

namespace CPU {
namespace MC68000 {

struct Microcycle {
	enum Operation {
		Read16,
		Write16,
		ReadHigh,
		ReadLow,
		WriteHigh,
		WriteLow,

		/// The data bus is not tristated, but no data request is made.
		Idle,

		/// No data bus interaction at all; bus is tristated.
		None
	};

	Operation operation = Operation::None;
	Cycles length = Cycles(2);
	uint32_t *address = nullptr;
	uint16_t *value = nullptr;
};

class BusHandler {
	public:
		Cycles perform_machine_cycle(const Microcycle &cycle) {
			return Cycles(0);
		}

		void flush();
};

#include "Implementation/68000Storage.hpp"

class ProcessorBase: public ProcessorStorage {
};

template <class T> class Processor: public ProcessorBase {
	public:
		void run_for(const Cycles cycles);

	private:
		T &bus_handler_;
};

}
}

#endif /* MC68000_h */
