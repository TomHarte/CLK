//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"

namespace Tandy::CoCo {

class DiskController final: public WD::WD1770 {
public:
	DiskController();

	void set_control(uint8_t);

	template <int address>
	uint8_t read() {
		if(address & 8) {
			return WD::WD1770::read(address);
		} else {
			return 0xff;
		}
	}

	template <int address>
	void write(const uint8_t value) {
		if(address & 8) {
			WD::WD1770::write(address, value);
		} else {
			set_control(value);
		}
	}
};

}
