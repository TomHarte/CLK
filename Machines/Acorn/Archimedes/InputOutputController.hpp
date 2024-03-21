//
//  InputOutputController.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Log.hpp"

#include "CMOSRAM.hpp"
#include "Keyboard.hpp"
#include "Sound.hpp"
#include "Video.hpp"


namespace Archimedes {

// IRQ A flags
namespace IRQA {
	// The first four of these are taken from the A500 documentation and may be inaccurate.
	static constexpr uint8_t PrinterBusy		= 0x01;
	static constexpr uint8_t SerialRinging		= 0x02;
	static constexpr uint8_t PrinterAcknowledge	= 0x04;
	static constexpr uint8_t VerticalFlyback	= 0x08;
	static constexpr uint8_t PowerOnReset		= 0x10;
	static constexpr uint8_t Timer0				= 0x20;
	static constexpr uint8_t Timer1				= 0x40;
	static constexpr uint8_t SetAlways			= 0x80;
}

// IRQ B flags
namespace IRQB {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t PoduleFIQRequest		= 0x01;
	static constexpr uint8_t SoundBufferPointerUsed	= 0x02;
	static constexpr uint8_t SerialLine				= 0x04;
	static constexpr uint8_t IDE					= 0x08;
	static constexpr uint8_t FloppyDiscInterrupt	= 0x10;
	static constexpr uint8_t PoduleIRQRequest		= 0x20;
	static constexpr uint8_t KeyboardTransmitEmpty	= 0x40;
	static constexpr uint8_t KeyboardReceiveFull	= 0x80;
}

// FIQ flags
namespace FIQ {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t FloppyDiscData			= 0x01;
	static constexpr uint8_t SerialLine				= 0x10;
	static constexpr uint8_t PoduleFIQRequest		= 0x40;
	static constexpr uint8_t SetAlways				= 0x80;
}

namespace InterruptRequests {
	static constexpr int IRQ = 0x01;
	static constexpr int FIQ = 0x02;
};

template <typename InterruptObserverT>
struct InputOutputController {
	int interrupt_mask() const {
		return
			((irq_a_.request() | irq_b_.request()) ? InterruptRequests::IRQ : 0) |
			(fiq_.request() ? InterruptRequests::FIQ : 0);
	}

	template <int c>
	bool tick_timer() {
		if(!counters_[c].value && !counters_[c].reload) {
			return false;
		}

		--counters_[c].value;
		if(!counters_[c].value) {
			counters_[c].value = counters_[c].reload;

			switch(c) {
				case 0:	return irq_a_.set(IRQA::Timer0);
				case 1:	return irq_a_.set(IRQA::Timer1);
				case 3: {
					serial_.shift();
					keyboard_.update();

					const uint8_t events = serial_.events(IOCParty);
					bool did_interrupt = false;
					if(events & HalfDuplexSerial::Receive) {
						did_interrupt |= irq_b_.set(IRQB::KeyboardReceiveFull);
					}
					if(events & HalfDuplexSerial::Transmit) {
						did_interrupt |= irq_b_.set(IRQB::KeyboardTransmitEmpty);
					}

					return did_interrupt;
				}
				default: break;
			}
			// TODO: events for timers 2 (baud).
		}

		return false;
	}

	void tick_timers() {
		bool did_change_interrupts = false;
		did_change_interrupts |= tick_timer<0>();
		did_change_interrupts |= tick_timer<1>();
		did_change_interrupts |= tick_timer<2>();
		did_change_interrupts |= tick_timer<3>();
		if(did_change_interrupts) {
			observer_.update_interrupts();
		}
	}

	static constexpr uint32_t AddressMask = 0x1f'ffff;

	bool read(uint32_t address, uint8_t &value) {
		const auto target = address & AddressMask;
		value = 0xff;
		switch(target) {
			default:
				logger.error().append("Unrecognised IOC read from %08x", address);
			break;

			case 0x3200000 & AddressMask:
				value = control_ | 0xc0;
				value &= ~(i2c_.clock() ? 2 : 0);
				value &= ~(i2c_.data() ? 1 : 0);
//				logger.error().append("IOC control read: C:%d D:%d", !(value & 2), !(value & 1));
			return true;

			case 0x3200004 & AddressMask:
				value = serial_.input(IOCParty);
				irq_b_.clear(IRQB::KeyboardReceiveFull);
				observer_.update_interrupts();
				logger.error().append("IOC keyboard receive: %02x", value);
			return true;

			// IRQ A.
			case 0x3200010 & AddressMask:
				value = irq_a_.status;
//				logger.error().append("IRQ A status is %02x", value);
			return true;
			case 0x3200014 & AddressMask:
				value = irq_a_.request();
				logger.error().append("IRQ A request is %02x", value);
			return true;
			case 0x3200018 & AddressMask:
				value = irq_a_.mask;
				logger.error().append("IRQ A mask is %02x", value);
			return true;

			// IRQ B.
			case 0x3200020 & AddressMask:
				value = irq_b_.status;
//				logger.error().append("IRQ B status is %02x", value);
			return true;
			case 0x3200024 & AddressMask:
				value = irq_b_.request();
				logger.error().append("IRQ B request is %02x", value);
			return true;
			case 0x3200028 & AddressMask:
				value = irq_b_.mask;
				logger.error().append("IRQ B mask is %02x", value);
			return true;

			// FIQ.
			case 0x3200030 & AddressMask:
				value = fiq_.status;
				logger.error().append("FIQ status is %02x", value);
			return true;
			case 0x3200034 & AddressMask:
				value = fiq_.request();
				logger.error().append("FIQ request is %02x", value);
			return true;
			case 0x3200038 & AddressMask:
				value = fiq_.mask;
				logger.error().append("FIQ mask is %02x", value);
			return true;

			// Counters.
			case 0x3200040 & AddressMask:
			case 0x3200050 & AddressMask:
			case 0x3200060 & AddressMask:
			case 0x3200070 & AddressMask:
				value = counters_[(target >> 4) - 0x4].output & 0xff;
//				logger.error().append("%02x: Counter %d low is %02x", target, (target >> 4) - 0x4, value);
			return true;

			case 0x3200044 & AddressMask:
			case 0x3200054 & AddressMask:
			case 0x3200064 & AddressMask:
			case 0x3200074 & AddressMask:
				value = counters_[(target >> 4) - 0x4].output >> 8;
//				logger.error().append("%02x: Counter %d high is %02x", target, (target >> 4) - 0x4, value);
			return true;
		}

		return true;
	}

	bool write(uint32_t address, uint8_t value) {
		const auto target = address & AddressMask;
		switch(target) {
			default:
				logger.error().append("Unrecognised IOC write of %02x at %08x", value, address);
			break;

			case 0x320'0000 & AddressMask:
				// TODO: does the rest of the control register relate to anything?
//				logger.error().append("TODO: IOC control write: C:%d D:%d", !(value & 2), !(value & 1));

				control_ = value;
				i2c_.set_clock_data(!(value & 2), !(value & 1));
			return true;

			case 0x320'0004 & AddressMask:
				logger.error().append("IOC keyboard transmit %02x", value);
				serial_.output(IOCParty, value);
				irq_b_.clear(IRQB::KeyboardTransmitEmpty);
				observer_.update_interrupts();
			return true;

			case 0x320'0014 & AddressMask:
				// b2: clear IF.
				// b3: clear IR.
				// b4: clear POR.
				// b5: clear TM[0].
				// b6: clear TM[1].
				irq_a_.clear(value & 0x7c);
				observer_.update_interrupts();
			return true;

			// Interrupts.
			case 0x320'0018 & AddressMask:	irq_a_.mask = value;	return true;
			case 0x320'0028 & AddressMask:	irq_b_.mask = value;	return true;
			case 0x320'0038 & AddressMask:	fiq_.mask = value;		return true;

			// Counters.
			case 0x320'0040 & AddressMask:
			case 0x320'0050 & AddressMask:
			case 0x320'0060 & AddressMask:
			case 0x320'0070 & AddressMask:
				counters_[(target >> 4) - 0x4].reload = uint16_t(
					(counters_[(target >> 4) - 0x4].reload & 0xff00) | value
				);
			return true;

			case 0x320'0044 & AddressMask:
			case 0x320'0054 & AddressMask:
			case 0x320'0064 & AddressMask:
			case 0x320'0074 & AddressMask:
				counters_[(target >> 4) - 0x4].reload = uint16_t(
					(counters_[(target >> 4) - 0x4].reload & 0x00ff) | (value << 8)
				);
			return true;

			case 0x320'0048 & AddressMask:
			case 0x320'0058 & AddressMask:
			case 0x320'0068 & AddressMask:
			case 0x320'0078 & AddressMask:
				counters_[(target >> 4) - 0x4].value = counters_[(target >> 4) - 0x4].reload;
			return true;

			case 0x320'004c & AddressMask:
			case 0x320'005c & AddressMask:
			case 0x320'006c & AddressMask:
			case 0x320'007c & AddressMask:
				counters_[(target >> 4) - 0x4].output = counters_[(target >> 4) - 0x4].value;
			return true;

			case 0x327'0000 & AddressMask:
				logger.error().append("TODO: exteded external podule space");
			return true;

			case 0x331'0000 & AddressMask:
				logger.error().append("TODO: 1772 / disk write");
			return true;

			case 0x335'0000 & AddressMask:
				logger.error().append("TODO: LS374 / printer data write");
			return true;

			case 0x335'0018 & AddressMask:
				logger.error().append("TODO: latch B write: %02x", value);
			return true;

			case 0x335'0040 & AddressMask:
				logger.error().append("TODO: latch A write: %02x", value);
			return true;

			case 0x335'0048 & AddressMask:
				logger.error().append("TODO: latch C write: %02x", value);
			return true;

			case 0x336'0000 & AddressMask:
				logger.error().append("TODO: podule interrupt request");
			return true;

			case 0x336'0004 & AddressMask:
				logger.error().append("TODO: podule interrupt mask");
			return true;

			case 0x33a'0000 & AddressMask:
				logger.error().append("TODO: 6854 / econet write");
			return true;

			case 0x33b'0000 & AddressMask:
				logger.error().append("TODO: 6551 / serial line write");
			return true;
		}

		return true;
	}

	InputOutputController(InterruptObserverT &observer) :
		observer_(observer),
		keyboard_(serial_),
		sound_(*this),
		video_(*this, sound_)
	{
		irq_a_.status = IRQA::SetAlways | IRQA::PowerOnReset;
		irq_b_.status = 0x00;
		fiq_.status = 0x80;				// 'set always'.

		i2c_.add_peripheral(&cmos_, 0xa0);
		update_interrupts();
	}

	Sound<InputOutputController> &sound() {
		return sound_;
	}

	Video<InputOutputController, Sound<InputOutputController>> &video() {
		return video_;
	}

	void update_interrupts() {
		if(sound_.interrupt()) {
			irq_b_.set(IRQB::SoundBufferPointerUsed);
		} else {
			irq_b_.clear(IRQB::SoundBufferPointerUsed);
		}

		if(video_.interrupt()) {
			irq_a_.set(IRQA::VerticalFlyback);
		}

		observer_.update_interrupts();
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &observer_;

	// IRQA, IRQB and FIQ states.
	struct Interrupt {
		uint8_t status, mask;
		uint8_t request() const {
			return status & mask;
		}
		bool set(uint8_t value) {
			status |= value;
			return status & mask;
		}
		void clear(uint8_t bits) {
			status &= ~bits;
		}
	};
	Interrupt irq_a_, irq_b_, fiq_;

	// The IOCs four counters.
	struct Counter {
		uint16_t value;
		uint16_t reload;
		uint16_t output;
	};
	Counter counters_[4];

	// The KART and keyboard beyond it.
	HalfDuplexSerial serial_;
	Keyboard keyboard_;

	// The control register.
	uint8_t control_ = 0xff;

	// The I2C bus.
	I2C::Bus i2c_;
	CMOSRAM cmos_;

	// Audio and video.
	Sound<InputOutputController> sound_;
	Video<InputOutputController, Sound<InputOutputController>> video_;
};

}

