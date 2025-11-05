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

	/// Call-in for the FIFOs; indicates that a FIFO just went from empty to not-empty,
	/// which might cause an interrupt elsewhere depending on the mask and on whether
	/// that interrupt is enabled.
	void fifo_has_data(const uint8_t mask) {
		apply_fifo_mask(mask, 0xff);
	}

	void fifo_is_empty(const uint8_t mask) {
		apply_fifo_mask(0x00, ~mask);
	}

	bool has_host_irq() const {
		return (flags_ & 0x01) && to_host4_.data_available();
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
			case 0:	return to_parasite1_.data_available() | to_host1_.not_full() | status();
			case 1:	return to_parasite1_.read();
			case 2:	return to_parasite2_.data_available() | to_host2_.not_full();
			case 3:	return to_parasite2_.read();
			case 4:	return to_parasite3_.data_available() | to_host3_.not_full();
			case 5:	return to_parasite3_.read();
			case 6:	return to_parasite4_.data_available() | to_host4_.not_full();
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
			case 0:	return to_host1_.data_available() | to_parasite1_.not_full() | status();
			case 1:	return to_host1_.read();
			case 2:	return to_host2_.data_available() | to_parasite2_.not_full();
			case 3:	return to_host2_.read();
			case 4:	return to_host3_.data_available() | to_parasite3_.not_full();
			case 5:	return to_host3_.read();
			case 6:	return to_host4_.data_available() | to_parasite4_.not_full();
			case 7:	return to_host4_.read();

			default: __builtin_unreachable();
		}
	}

	void set_reset(const bool reset) {
		if(reset_ == reset) {
			return;
		}

		// This is a software approximtion of holding the reset state for as long
		// as it is signalled.
		if(!reset) {
			flags_ = 0x01;
			to_parasite1_.reset();
			to_parasite2_.reset();
			to_parasite3_.reset();
			to_parasite4_.reset();
			to_host1_.reset();
			to_host2_.reset();
			to_host3_.reset();
			to_host4_.reset();
		}
		reset_ = reset;

		update_parasite_reset();
	}

private:
	void signal_changes(const uint8_t changes) {
		if(changes & 0x01) {
			host_.set_host_tube_irq(interrupt_sources_ & 0x01);
		}
		if(changes & 0x06) {
			host_.set_parasite_tube_irq(interrupt_sources_ & 0x06);
		}
		if(changes & 0x08) {
			host_.set_parasite_tube_nmi(interrupt_sources_ & 0x08);
		}
	}

	uint8_t signalling_fifos() const {
		return interrupt_sources_ & flags_;
	}

	void apply_fifo_mask(const uint8_t or_, const uint8_t and_) {
		const auto signalling = signalling_fifos();
		interrupt_sources_ = (interrupt_sources_ | or_) & and_;
		signal_changes(signalling_fifos() ^ signalling);
	}

	void update_parasite_reset() {
		host_.set_parasite_reset((flags_ & 0x20) || reset_);
	}

	uint8_t status() const {
		return flags_;
	}

	void set_status(const uint8_t value) {
		const auto signalling = signalling_fifos();
		const uint8_t bits = value & 0x3f;
		if(value & 0x80) {
			flags_ |= bits;
		} else {
			flags_ &= ~bits;
		}
		signal_changes(signalling_fifos() ^ signalling);
		if(value & 0x20) {
			update_parasite_reset();
		}

		// TODO: understand meaning of bits 4 and 6.
	}

	HostT &host_;
	uint8_t flags_ = 0x01;
	bool reset_ = false;
	uint8_t interrupt_sources_ = 0x00;

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
