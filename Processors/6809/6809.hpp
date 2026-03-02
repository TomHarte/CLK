//
//  6809.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Registers.hpp"

namespace CPU::M6809 {

enum class BusState {
	Normal = 0b00,
	InterruptOrResetAcknowledge = 0b01,
	SyncAcknowledge = 0b10,
	HaltOrBusGrantAcknowledge = 0b11,
};

enum class ReadWrite {
	Read,
	Write
};

tempalte <typename Traits>
struct Processor {
	Processor(Traits::BusHandlerT &bus_handler) noexcept : bus_handler_(bus_handler) {}

	void run_for(const Cycles cycles) {
		static constexpr auto FirstCounter = __COUNTER__;

	}

private:
	Traits::BusHandlerT &bus_handler_;
	Cycles cycles_;
};

}
