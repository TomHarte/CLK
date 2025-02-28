//
//  PIA.h
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/6532/6532.hpp"

#include <cstdint>

namespace Atari2600 {

class PIA: public MOS::MOS6532<PIA> {
public:
	inline uint8_t get_port_input(const int port) {
		return port_values_[port];
	}

	inline void update_port_input(const int port, const uint8_t mask, const bool set) {
		if(set) port_values_[port] &= ~mask; else port_values_[port] |= mask;
		set_port_did_change(port);
	}

	PIA() : port_values_{0xff, 0xff} {}

private:
	uint8_t port_values_[2];
};

}
