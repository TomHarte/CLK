//
//  Interrupts.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Processors/6502/6502.hpp"

namespace Commodore::Plus4 {

struct BusController {
	virtual void set_irq_line(bool) = 0;
	virtual void set_ready_line(bool) = 0;
};

struct Interrupts {
public:
	Interrupts(BusController &delegate) : delegate_(delegate) {}
	BusController &bus() {
		return delegate_;
	}

	enum Flag {
		Timer3 = 0x40,
		Timer2 = 0x10,
		Timer1 = 0x08,
		Raster = 0x02,
	};

	uint8_t status() const {
		return status_ | ((status_ & mask_) ? 0x80 : 0x00) | 0x21;
	}

	uint8_t mask() const {
		return mask_;
	}

	void set_status(const uint8_t status) {
		status_ &= ~status;
		update_output();
	}

	void apply(const uint8_t interrupt) {
		status_ |= interrupt;
		update_output();
	}

	void set_mask(const uint8_t mask) {
		mask_ = mask;
		update_output();
	}

private:
	void update_output() {
		const bool set = status_ & mask_;
		if(set != last_set_) {
			delegate_.set_irq_line(set);
			last_set_ = set;
		}
	}

	BusController &delegate_;
	uint8_t status_;
	uint8_t mask_;
	bool last_set_ = false;
};

}
