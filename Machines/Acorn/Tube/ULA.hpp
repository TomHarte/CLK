//
//  ULA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "FIFO.hpp"

namespace Acorn::Tube {

/*!
	The non-FIFO section of the tube ULA.
*/
template <typename HostT, typename ParasiteT>
struct ULA {
	ULA(HostT &host, ParasiteT &parasite) :
		host_(host),
		parasite_(parasite),
		to_parasite1_(*this, 0x02),
		to_parasite2_(*this),
		to_parasite3_(*this, 0x08),
		to_parasite4_(*this, 0x04),
		to_host1_(*this),
		to_host2_(*this),
		to_host3_(*this),
		to_host4_(*this, 0x01)
	{}

	uint8_t status() const {
		return flags_;
	}

	void set_status(const uint8_t value) {
		const uint8_t bits = value & 0x3f;
		if(value & 0x80) {
			flags_ |= bits;
		} else {
			flags_ &= ~bits;
		}
	}

	void fifo_has_data(const uint8_t mask) {
		if(!(flags_ & mask)) return;

		switch(mask) {
			default: __builtin_unreachable();
			case 0x01:
				host_.set_tube_irq();
			break;
			case 0x02:
			case 0x04:
				parasite_.set_tube_irq();
			break;
			case 0x08:
				parasite_.set_tube_nmi();
			break;
		}
	}

	bool has_host_irq() const {
		return (flags_ & 0x01) && (to_host1_.status() & 0x80);
	}

private:
	HostT &host_;
	ParasiteT &parasite_;
	uint8_t flags_ = 0x3f;

	FIFO<1, ULA> to_parasite1_;
	FIFO<1, ULA> to_parasite2_;
	FIFO<2, ULA> to_parasite3_;
	FIFO<1, ULA> to_parasite4_;

	FIFO<24, ULA> to_host1_;
	FIFO<1, ULA> to_host2_;
	FIFO<2, ULA> to_host3_;
	FIFO<1, ULA> to_host4_;
};

}
