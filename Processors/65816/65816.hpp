//
//  65816.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef WDC65816_hpp
#define WDC65816_hpp

#include <cstdint>
#include <vector>

#include "../RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace CPU {
namespace WDC65816 {

#include "Implementation/65816Storage.hpp"

template <typename BusHandler> class Processor: private ProcessorStorage {
	public:
		Processor(BusHandler &bus_handler) : bus_handler_(bus_handler) {}

		void run_for(const Cycles cycles);

	private:
		BusHandler &bus_handler_;
};

#include "Implementation/65816Implementation.hpp"

}
}

#endif /* WDC65816_hpp */
