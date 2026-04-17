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
			static constexpr auto IRQMask = IRQ1 | IRQ2;
			ports_[port].control = (ports_[port].control & IRQMask) | (value & ~IRQMask);

			if(ports_[port].control & Flag::C2IsOutput) {
				if(ports_[port].control & Flag::C2OutputNotStrobe) {
					port_handler_.template observe<port ? Control::CB2 : Control::CA2>(
						bool(ports_[port].control & Flag::C2NonStrobeOutput)
					);
				} else {
					printf("UNIMPLEMENTED: strobed output\n");
				}
			}
		} else {
			if(ports_[port].control & Flag::DataVisible) {
				ports_[port].data = value;
			} else {
				ports_[port].direction = value;
			}

			update_output<Port(port)>();
		}
	}

	template <int address>
	uint8_t read() {
		static constexpr int port = bool(address & RS1Mask);
		if constexpr(address & RS0Mask) {
			const auto result = ports_[port].control;
			ports_[port].clear_irq();
			update_interrupts();
			return result;
		} else {
			if(ports_[port].control & Flag::DataVisible) {
				return ports_[port].input(port_handler_.template input<Port(port)>());
			} else {
				return ports_[port].direction;
			}
		}
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

		static constexpr bool is_irq2 = control == Control::CB2 || control == Control::CA2;	// 0 = IRQ1; 1 = IRQ2.
		static constexpr int port = control == Control::CB2 || control == Control::CB1;		// 0 = port A; 1 = port B.

		// Reject any set to C[A/B]2 if it's in output mode.
		if constexpr (is_irq2) {
			if(ports_[port].control & Flag::C2IsOutput) {
				return;
			}
		}
		port_handler_.template observe<control>(value);

		// Test whether change was in the triggering direction; if so update control bit and check for change
		// to interrupt output.
		static constexpr int direction = is_irq2 ? IRQ2Direction : IRQ1Direction;
		if(value != bool(ports_[port].control & direction)) {
			ports_[port].apply_irq(is_irq2 ? Flag::IRQ2 : Flag::IRQ1);
			update_interrupts();
		}
	}

	template <Port port>
	void set_floating(const uint8_t value) {
		ports_[int(port)].floating = value;
		update_output<port>();
	}

private:
	template <Port port>
	void update_output() {
		const uint8_t output = ports_[int(port)].output();
		if(output != ports_[int(port)].previous_output) {
			ports_[int(port)].previous_output = output;
			port_handler_.template output<port>(output);
		}
	}

	PortHandlerT &port_handler_;
	bool inputs_[4]{};

	enum Flag: uint8_t {
		DataVisible			= 0b0000'0100,
		C2IsOutput			= 0b0010'0000,

		C2OutputNotStrobe	= 0b0001'0000,
		C2StrobeERestore	= 0b0000'1000,
		C2NonStrobeOutput	= 0b0000'1000,

		IRQ1				= 0b1000'0000,
		IRQ1Direction		= 0b0000'0010,
		EnableIRQ1			= 0b0000'0001,

		IRQ2				= 0b0100'0000,
		IRQ2Direction		= 0b0001'0000,
		EnableIRQ2			= 0b0000'1000,
	};

	struct {
		uint8_t control = 0;
		uint8_t data = 0;
		uint8_t direction = 0;	// Per bit: 0 = input; 1 = output.
		uint8_t floating = 0xff;

		void apply_irq(const uint8_t mask) {
			control |= mask;
		}

		void clear_irq() {
			control &= ~(IRQ1 | IRQ2);
		}

		bool irq() const {
			static constexpr auto IRQ1Value = EnableIRQ1 | IRQ1;
			static constexpr auto IRQ1Mask = IRQ1Value;

			static constexpr auto IRQ2Value = EnableIRQ2 | IRQ2;
			static constexpr auto IRQ2Mask = IRQ2Value | C2IsOutput;

			return
				((control & IRQ1Mask) == IRQ1Value) ||
				((control & IRQ2Mask) == IRQ2Value);
		}

		mutable uint8_t previous_output = 0x00;
		uint8_t output() const {
			return uint8_t(
				(data & direction) |
				(floating & ~direction)
			);
		}
		uint8_t input(const uint8_t bus) const {
			return (data & direction) | (bus & ~direction);
		}
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
		const bool irqa = ports_[0].irq();
		const bool irqb = ports_[1].irq();

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
