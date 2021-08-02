//
//  6526Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Implementation_h
#define _526Implementation_h

#include <cassert>
#include <cstdio>

namespace MOS {
namespace MOS6526 {

template <typename BusHandlerT, Personality personality>
template <int port> void MOS6526<BusHandlerT, personality>::set_port_output() {
	const uint8_t output = output_[port] | (~data_direction_[port]);
	port_handler_.set_port_output(Port(port), output);
}

template <typename BusHandlerT, Personality personality>
template <int port> uint8_t MOS6526<BusHandlerT, personality>::get_port_input() {
	const uint8_t input = port_handler_.get_port_input(Port(port));
	return (input & ~data_direction_[port]) | (output_[port] & data_direction_[port]);
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::posit_interrupt(uint8_t mask) {
	if(!mask) {
		return;
	}
	interrupt_state_ |= mask;
	update_interrupts();
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::update_interrupts() {
	if(interrupt_state_ & interrupt_control_) {
		pending_ |= InterruptInOne;
	}
}

template <typename BusHandlerT, Personality personality>
bool MOS6526<BusHandlerT, personality>::get_interrupt_line() {
	return interrupt_state_ & 0x80;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::write(int address, uint8_t value) {
	address &= 0xf;
	switch(address) {
		// Port output.
		case 0:
			output_[0] = value;
			set_port_output<0>();
		break;
		case 1:
			output_[1] = value;
			set_port_output<1>();
		break;

		// Port direction.
		case 2:
			data_direction_[0] = value;
			set_port_output<0>();
		break;
		case 3:
			data_direction_[1] = value;
			set_port_output<1>();
		break;

		// Counters; writes set the reload values.
		case 4:	counter_[0].template set_reload<0>(value);	break;
		case 5:	counter_[0].template set_reload<8>(value);	break;
		case 6:	counter_[1].template set_reload<0>(value);	break;
		case 7:	counter_[1].template set_reload<8>(value);	break;

		// Time-of-day clock.
		//
		// 8520: a binary counter; stopped on any write, restarted
		// upon a write to the LSB.
		case 8:
			if constexpr (personality == Personality::P8250) {
				if(counter_[1].control & 0x80) {
					tod_alarm_ = (tod_alarm_ & 0xffff00) | uint32_t(value);
				} else {
					tod_ = (tod_ & 0xffff00) | uint32_t(value);
					tod_increment_mask_ = uint32_t(~0);
				}
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;
		case 9:
			if constexpr (personality == Personality::P8250) {
				if(counter_[1].control & 0x80) {
					tod_alarm_ = (tod_alarm_ & 0xff00ff) | uint32_t(value << 8);
				} else {
					tod_ = (tod_ & 0xff00ff) | uint32_t(value << 8);
					tod_increment_mask_ = 0;
				}
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;
		case 10:
			if constexpr (personality == Personality::P8250) {
				if(counter_[1].control & 0x80) {
					tod_alarm_ = (tod_alarm_ & 0x00ffff) | uint32_t(value << 16);
				} else {
					tod_ = (tod_ & 0x00ffff) | uint32_t(value << 16);
					tod_increment_mask_ = 0;
				}
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;

		case 11:
			if constexpr (personality != Personality::P8250) {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;

		// Interrupt control.
		case 13: {
			if(value & 0x80) {
				interrupt_control_ |= value & 0x7f;
			} else {
				interrupt_control_ &= ~(value & 0x7f);
			}
			update_interrupts();
		} break;

		// Control.
		case 14:	counter_[0].template set_control<false>(value);	break;
		case 15:	counter_[1].template set_control<true>(value);	break;

		default:
			printf("Unhandled 6526 write: %02x to %d\n", value, address);
			assert(false);
		break;
	}
}

template <typename BusHandlerT, Personality personality>
uint8_t MOS6526<BusHandlerT, personality>::read(int address) {
	address &= 0xf;
	switch(address) {
		case 0:		return get_port_input<0>();
		case 1:		return get_port_input<1>();

		case 2: case 3:
		return data_direction_[address - 2];

		// Counters; reads obtain the current values.
		case 4:	return uint8_t(counter_[0].value >> 0);
		case 5:	return uint8_t(counter_[0].value >> 8);
		case 6:	return uint8_t(counter_[1].value >> 0);
		case 7:	return uint8_t(counter_[1].value >> 8);

		// Interrupt state.
		case 13: {
			const uint8_t result = interrupt_state_;
			interrupt_state_ = 0;
			update_interrupts();
			return result;
		} break;

		case 14: case 15:
		return counter_[address - 14].control;

		// Time-of-day clock.
		//
		// 8250: Latch on MSB. Unlatch on LSB. Read raw if not latched.
		case 8:
			if constexpr (personality == Personality::P8250) {
				if(tod_latch_) {
					const uint8_t result = tod_latch_ & 0xff;
					tod_latch_ = 0;
					return result;
				} else {
					return tod_ & 0xff;
				}
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;
		case 9:
			if constexpr (personality == Personality::P8250) {
				if(tod_latch_) {
					return (tod_latch_ >> 8) & 0xff;
				} else {
					return (tod_ >> 8) & 0xff;
				}
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;
		case 10:
			if constexpr (personality == Personality::P8250) {
				tod_latch_ = tod_ | 0xff00'0000;
				return (tod_ >> 16) & 0xff;
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);
			}
		break;

		case 11:
			if constexpr (personality == Personality::P8250) {
				return 0x00;	// Assumed. Just a guss.
			} else {
				printf("6526 TOD clock not implemented\n");
				assert(false);

			}
		break;

		default:
			printf("Unhandled 6526 read from %d\n", address);
			assert(false);
		break;
	}
	return 0xff;
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::run_for(const HalfCycles half_cycles) {
	half_divider_ += half_cycles;
	int sub = half_divider_.divide_cycles().template as<int>();

	while(sub--) {
		pending_ <<= 1;
		if(pending_ & InterruptNow) {
			interrupt_state_ |= 0x80;
		}
		pending_ &= PendingClearMask;

		// TODO: use CNT potentially to clock timer A, elimiante conditional above.
		counter_[0].advance(false);
		counter_[1].advance(counter_[0].hit_zero);
		posit_interrupt((counter_[0].hit_zero ? 0x01 : 0x00) | (counter_[1].hit_zero ? 0x02 : 0x00));
	}
}

template <typename BusHandlerT, Personality personality>
void MOS6526<BusHandlerT, personality>::advance_tod(int count) {
	if(!count) return;

	if constexpr(personality == Personality::P8250) {
		// The 8250 uses a simple binary counter to replace the
		// 6526's time-of-day clock. So this is easy.
		const uint32_t distance_to_alarm_ = (tod_alarm_ - tod_) & 0xffffff;
		tod_ += uint32_t(count) & tod_increment_mask_;
		if(distance_to_alarm_ <= uint32_t(count)) {
			posit_interrupt(0x04);
		}
	} else {
		// The 6526 uses a time-of-day clock. This may or may not
		// be accurate.
	}
}

}
}

#endif /* _526Implementation_h */
