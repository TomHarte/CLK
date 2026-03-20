//
//  6821.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef _821_hpp
#define _821_hpp

namespace Motorola::MC6821 {

enum class Port {
	A = 0,
	B = 1,
};

enum class IRQ {
	A,
	B,
};

enum class Control {
	CA1,	CA2,
	CB1,	CB2,
};

template <typename PortHandlerT, int RS0Mask = 1, int RS1Mask = 2>
class MC6821 {
public:
	MC6821(PortHandlerT &port_handler) : port_handler_(port_handler) {}

	template <int address>
	void write(const uint8_t value) {
		static constexpr int port = bool(address & RS1Mask);

		if constexpr (address & RS0Mask) {
			ports_[port].control = value;
			update_interrupts();
		} else {
			if(ports_[port].control & Flag::DataVisible) {
				ports_[port].data = value;
			} else {
				ports_[port].direction = value;
			}

			const uint8_t output = ports_[port].output();
			if(output != ports_[port].previous_output) {
				ports_[port].previous_output = output;
				port_handler_.template output<Port(port)>(output);
			}
		}
	}

	template <int address>
	uint8_t read() {
		static constexpr int port = (address >> 1) & 1;
		if constexpr(address & 1) {
			return ports_[port].control;
		} else {
			if(ports_[port].control & Flag::DataVisible) {
				ports_[port].control &= ~(IRQA | IRQB);
				update_interrupts();
				return ports_[port].input(port_handler_.template input<Port(port)>());
			} else {
				return ports_[port].direction;
			}
		}
		return 0;
	}

	void refresh() const {
		refresh<Port::A>();
		refresh<Port::B>();
	}

	template <Control control>
	void set(const bool value) {
		// Reject anything that isn't a change.
		static constexpr int input = int(control);
		if(inputs_[input] == value) {
			return;
		}
		inputs_[input] = value;
		port_handler_.template observe<control>(value);

		static constexpr bool is_irqb = control == Control::CB2 || control == Control::CA2;
		static constexpr int port = control == Control::CB2 || control == Control::CB1;

		// Reject any set to CB2 if it's in output mode.
		if constexpr (is_irqb) {
			if(ports_[port].control & Flag::C2IsOutput) {
				return;
			}
		}

		// Test whether change was in the triggering direction; if so update control bit and check for change
		// to interrupt output.
		static constexpr int direction = is_irqb ? IRQBDirection : IRQADirection;
		if(value != bool(ports_[port].control & direction)) {
			ports_[port].control |= is_irqb ? Flag::IRQB : Flag::IRQA;
			update_interrupts();
		}
	}

private:
	PortHandlerT &port_handler_;
	bool inputs_[4]{};

	enum Flag: uint8_t {
		IRQA			= 0b1000'0000,
		IRQB			= 0b0100'0000,
		C2				= 0b0011'1000,
		DataVisible		= 0b0000'0100,
		C1				= 0b0000'0011,

		C2IsOutput		= 0b0010'0000,

		IRQADirection	= 0b0000'0010,
		EnableIRQA		= 0b0000'0001,

		IRQBDirection	= 0b0001'0000,
		EnableIRQB		= 0b0000'1000,
	};

	struct {
		uint8_t control = 0;
		uint8_t data = 0;
		uint8_t direction = 0;	// Per bit: 0 = input; 1 = output.

		bool irqa() const {
			return (control & (IRQA | EnableIRQA)) == (IRQA | EnableIRQA);
		}
		bool irqb() const {
			return (control & (IRQB | EnableIRQB | C2IsOutput)) == (IRQB | EnableIRQB);
		}

		uint8_t output() const {
			return data | ~direction;
		}
		uint8_t input(const uint8_t bus) const {
			return (data & direction) | (bus & ~direction);
		}
		mutable uint8_t previous_output = 0x00;
	} ports_[2];

	template <Port port>
	void refresh() const {
		static constexpr auto index = int(port);

		const uint8_t output = ports_[index].output();
		ports_[index].previous_output = output;
		port_handler_.template output<port>(output);
	}

	bool irqa_ = false, irqb_ = false;
	void update_interrupts() {
		const bool irqa = ports_[0].irqa() || ports_[1].irqa();
		const bool irqb = ports_[0].irqb() || ports_[1].irqb();

		if(irqa_ != irqa) {
			irqa_ = irqa;
			port_handler_.template set<IRQ::A>(irqa);
		}
		if(irqb_ != irqb) {
			irqb_ = irqb;
			port_handler_.template set<IRQ::B>(irqb);
		}
	}
};

};

#endif /* _821_hpp */
