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
template <typename HostT>
struct ULA {
	ULA(HostT &host) :
		host_(host),
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
				host_.set_host_tube_irq();
			break;
			case 0x02:
			case 0x04:
				host_.set_parasite_tube_irq();
			break;
			case 0x08:
				host_.set_parasite_tube_nmi();
			break;
		}
	}

	bool has_host_irq() const {
		return (flags_ & 0x01) && to_host1_.data_available();
	}

	bool has_parasite_irq() const {
		return
			((flags_ & 0x02) && to_parasite1_.data_available()) ||
			((flags_ & 0x04) && to_parasite4_.data_available());
	}

	bool has_parasite_nmi() const {
		return (flags_ & 0x08) && to_parasite3_.data_available();
	}

	void parasite_write(const uint16_t address, const uint8_t value) {
		switch(address & 7) {
			case 1:	to_host1_.write(value);	break;
			case 3:	to_host2_.write(value);	break;
			case 5:	to_host3_.write(value);	break;
			case 7:	to_host4_.write(value);	break;
			default: break;
		}
	}

	uint8_t parasite_read(const uint16_t address) {
		switch(address & 7) {
			case 0:	return to_parasite1_.data_available() | to_host1_.full() | status();
			case 1:	return to_parasite1_.read();
			case 2:	return to_parasite2_.data_available() | to_host2_.full();
			case 3:	return to_parasite2_.read();
			case 4:	return to_parasite3_.data_available() | to_host3_.full();
			case 5:	return to_parasite3_.read();
			case 6:	return to_parasite4_.data_available() | to_host4_.full();
			case 7:	return to_parasite4_.read();

			default: __builtin_unreachable();
		}
	}

	void host_write(const uint16_t address, const uint8_t value) {
		switch(address & 7) {
			case 0: set_status(value);			break;
			case 1:	to_parasite1_.write(value);	break;
			case 3:	to_parasite2_.write(value);	break;
			case 5:	to_parasite3_.write(value);	break;
			case 7:	to_parasite4_.write(value);	break;
			default: break;
		}
	}

	uint8_t host_read(const uint16_t address) {
		switch(address & 7) {
			case 0:	return to_host1_.data_available() | to_parasite1_.full() | status();
			case 1:	return to_host1_.read();
			case 2:	return to_host2_.data_available() | to_parasite2_.full();
			case 3:	return to_host2_.read();
			case 4:	return to_host3_.data_available() | to_parasite3_.full();
			case 5:	return to_host3_.read();
			case 6:	return to_host4_.data_available() | to_parasite4_.full();
			case 7:	return to_host4_.read();

			default: __builtin_unreachable();
		}
	}
private:
	HostT &host_;
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
