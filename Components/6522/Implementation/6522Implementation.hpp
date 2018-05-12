//
//  Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

template <typename T> void MOS6522<T>::set_register(int address, uint8_t value) {
	address &= 0xf;
	switch(address) {
		case 0x0:
			registers_.output[1] = value;
			bus_handler_.set_port_output(Port::B, value, registers_.data_direction[1]);	// TODO: handshake

			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | ((registers_.peripheral_control&0x20) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;
		case 0xf:
		case 0x1:
			registers_.output[0] = value;
			bus_handler_.set_port_output(Port::A, value, registers_.data_direction[0]);	// TODO: handshake

			registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | ((registers_.peripheral_control&0x02) ? 0 : InterruptFlag::CB2ActiveEdge));
			reevaluate_interrupts();
		break;

		case 0x2:
			registers_.data_direction[1] = value;
		break;
		case 0x3:
			registers_.data_direction[0] = value;
		break;

		// Timer 1
		case 0x6:	case 0x4:	registers_.timer_latch[0] = (registers_.timer_latch[0]&0xff00) | value;	break;
		case 0x5:	case 0x7:
			registers_.timer_latch[0] = (registers_.timer_latch[0]&0x00ff) | static_cast<uint16_t>(value << 8);
			registers_.interrupt_flags &= ~InterruptFlag::Timer1;
			if(address == 0x05) {
				registers_.next_timer[0] = registers_.timer_latch[0];
				timer_is_running_[0] = true;
			}
			reevaluate_interrupts();
		break;

		// Timer 2
		case 0x8:	registers_.timer_latch[1] = value;	break;
		case 0x9:
			registers_.interrupt_flags &= ~InterruptFlag::Timer2;
			registers_.next_timer[1] = registers_.timer_latch[1] | static_cast<uint16_t>(value << 8);
			timer_is_running_[1] = true;
			reevaluate_interrupts();
		break;

		// Shift
		case 0xa:	registers_.shift = value;				break;

		// Control
		case 0xb:
			registers_.auxiliary_control = value;
		break;
		case 0xc:
//			printf("Peripheral control %02x\n", value);
			registers_.peripheral_control = value;

			// TODO: simplify below; trying to avoid improper logging of unimplemented warnings in input mode
			if(value & 0x08) {
				switch(value & 0x0e) {
					default: printf("Unimplemented control line mode %d\n", (value >> 1)&7);		break;
					case 0x0c:	bus_handler_.set_control_line_output(Port::A, Line::Two, false);	break;
					case 0x0e:	bus_handler_.set_control_line_output(Port::A, Line::Two, true);		break;
				}
			}
			if(value & 0x80) {
				switch(value & 0xe0) {
					default: printf("Unimplemented control line mode %d\n", (value >> 5)&7);		break;
					case 0xc0:	bus_handler_.set_control_line_output(Port::B, Line::Two, false);	break;
					case 0xe0:	bus_handler_.set_control_line_output(Port::B, Line::Two, true);		break;
				}
			}
		break;

		// Interrupt control
		case 0xd:
			registers_.interrupt_flags &= ~value;
			reevaluate_interrupts();
		break;
		case 0xe:
			if(value&0x80)
				registers_.interrupt_enable |= value;
			else
				registers_.interrupt_enable &= ~value;
			reevaluate_interrupts();
		break;
	}
}

template <typename T> uint8_t MOS6522<T>::get_register(int address) {
	address &= 0xf;
	switch(address) {
		case 0x0:
			registers_.interrupt_flags &= ~(InterruptFlag::CB1ActiveEdge | InterruptFlag::CB2ActiveEdge);
			reevaluate_interrupts();
		return get_port_input(Port::B, registers_.data_direction[1], registers_.output[1]);
		case 0xf:	// TODO: handshake, latching
		case 0x1:
			registers_.interrupt_flags &= ~(InterruptFlag::CA1ActiveEdge | InterruptFlag::CA2ActiveEdge);
			reevaluate_interrupts();
		return get_port_input(Port::A, registers_.data_direction[0], registers_.output[0]);

		case 0x2:	return registers_.data_direction[1];
		case 0x3:	return registers_.data_direction[0];

		// Timer 1
		case 0x4:
			registers_.interrupt_flags &= ~InterruptFlag::Timer1;
			reevaluate_interrupts();
		return registers_.timer[0] & 0x00ff;
		case 0x5:	return registers_.timer[0] >> 8;
		case 0x6:	return registers_.timer_latch[0] & 0x00ff;
		case 0x7:	return registers_.timer_latch[0] >> 8;

		// Timer 2
		case 0x8:
			registers_.interrupt_flags &= ~InterruptFlag::Timer2;
			reevaluate_interrupts();
		return registers_.timer[1] & 0x00ff;
		case 0x9:	return registers_.timer[1] >> 8;

		case 0xa:	return registers_.shift;

		case 0xb:	return registers_.auxiliary_control;
		case 0xc:	return registers_.peripheral_control;

		case 0xd:	return registers_.interrupt_flags | (get_interrupt_line() ? 0x80 : 0x00);
		case 0xe:	return registers_.interrupt_enable | 0x80;
	}

	return 0xff;
}

template <typename T> uint8_t MOS6522<T>::get_port_input(Port port, uint8_t output_mask, uint8_t output) {
	uint8_t input = bus_handler_.get_port_input(port);
	return (input & ~output_mask) | (output & output_mask);
}

template <typename T> T &MOS6522<T>::bus_handler() {
	return bus_handler_;
}

// Delegate and communications
template <typename T> void MOS6522<T>::reevaluate_interrupts() {
	bool new_interrupt_status = get_interrupt_line();
	if(new_interrupt_status != last_posted_interrupt_status_) {
		last_posted_interrupt_status_ = new_interrupt_status;
		bus_handler_.set_interrupt_status(new_interrupt_status);
	}
}
