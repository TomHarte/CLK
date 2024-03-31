//
//  InputOutputController.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "CMOSRAM.hpp"
#include "Keyboard.hpp"
#include "Sound.hpp"
#include "Video.hpp"

#include "../../../Outputs/Log.hpp"
#include "../../../Activity/Observer.hpp"

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

template <typename InterruptObserverT, typename ClockRateObserverT>
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

	/// Decomposes an Archimedes bus address into bank, offset and type.
	struct Address {
		constexpr Address(uint32_t bus_address) noexcept {
			bank = (bus_address >> 16) & 0b111;
			type = Type((bus_address >> 19) & 0b11);
			offset = bus_address & 0b1111100;
		}

		/// A value from 0 to 7 indicating the device being addressed.
		uint32_t bank;
		/// A seven-bit value which is a multiple of 4, indicating the address within the bank.
		uint32_t offset;
		/// Access type.
		enum class Type {
			Sync = 0b00,
			Fast = 0b10,
			Medium = 0b01,
			Slow = 0b11
		} type;
	};

	// Peripheral addresses on the A500:
	//
	//	fast/1	=	FDC
	//	sync/2	=	econet
	//	sync/3	= 	serial line
	//
	//	bank 4	=	podules
	//
	//	fast/5

	template <typename IntT>
	bool read(uint32_t address, IntT &destination) {
		const Address target(address);

		const auto set_byte = [&](uint8_t value) {
			if constexpr (std::is_same_v<IntT, uint32_t>) {
				destination = static_cast<uint32_t>(value << 16) | 0xff'00'ff'ff;
			} else {
				destination = value;
			}
		};

		// TODO: flatten the switch below, and the equivalent in `write`.

		switch(target.bank) {
			default:
				logger.error().append("Unrecognised IOC read from %08x i.e. bank %d / type %d", address, target.bank, target.type);
				destination = IntT(~0);
			break;

			// Bank 0: internal registers.
			case 0:
				switch(target.offset) {
					default:
						logger.error().append("Unrecognised IOC bank 0 read; offset %02x", target.offset);
					break;

					case 0x00: {
						uint8_t value = control_ | 0xc0;
						value &= ~(i2c_.clock() ? 2 : 0);
						value &= ~(i2c_.data() ? 1 : 0);
						set_byte(value);
//						logger.error().append("IOC control read: C:%d D:%d", !(value & 2), !(value & 1));
					} break;

					case 0x04:
						set_byte(serial_.input(IOCParty));
						irq_b_.clear(IRQB::KeyboardReceiveFull);
						observer_.update_interrupts();
//						logger.error().append("IOC keyboard receive: %02x", value);
					break;

					// IRQ A.
					case 0x10:
						set_byte(irq_a_.status);
//						logger.error().append("IRQ A status is %02x", value);
					break;
					case 0x14:
						set_byte(irq_a_.request());
//						logger.error().append("IRQ A request is %02x", value);
					break;
					case 0x18:
						set_byte(irq_a_.mask);
//						logger.error().append("IRQ A mask is %02x", value);
					break;

					// IRQ B.
					case 0x20:
						set_byte(irq_b_.status);
//						logger.error().append("IRQ B status is %02x", value);
					break;
					case 0x24:
						set_byte(irq_b_.request());
//						logger.error().append("IRQ B request is %02x", value);
					break;
					case 0x28:
						set_byte(irq_b_.mask);
//						logger.error().append("IRQ B mask is %02x", value);
					break;

					// FIQ.
					case 0x30:
						set_byte(fiq_.status);
						logger.error().append("FIQ status is %02x", fiq_.status);
					break;
					case 0x34:
						set_byte(fiq_.request());
						logger.error().append("FIQ request is %02x", fiq_.request());
					break;
					case 0x38:
						set_byte(fiq_.mask);
						logger.error().append("FIQ mask is %02x", fiq_.mask);
					break;

					// Counters.
					case 0x40:	case 0x50:	case 0x60:	case 0x70:
						set_byte(counters_[(target.offset >> 4) - 0x4].output & 0xff);
//						logger.error().append("%02x: Counter %d low is %02x", target, (target >> 4) - 0x4, value);
					break;

					case 0x44:	case 0x54:	case 0x64:	case 0x74:
						set_byte(counters_[(target.offset >> 4) - 0x4].output >> 8);
//						logger.error().append("%02x: Counter %d high is %02x", target, (target >> 4) - 0x4, value);
					break;
				}
			break;
		}

		return true;
	}

	template <typename IntT>
	bool write(uint32_t address, IntT bus_value) {
		const Address target(address);

		// Empirically, RISC OS 3.19:
		//	* at 03801e88 and 03801e8c loads R8 and R9 with 0xbe0000 and 0xff0000 respectively; and
		//	* subsequently uses 32-bit strs (e.g. at 03801eac) to write those values to latch A.
		//
		// Given that 8-bit ARM writes duplicate the 8-bit value four times across the data bus,
		// my conclusion is that the IOC is probably connected to data lines 15–23.
		//
		// Hence: use @c byte to get a current 8-bit value.
		const auto byte = [](IntT original) -> uint8_t {
			if constexpr (std::is_same_v<IntT, uint32_t>) {
				return static_cast<uint8_t>(original >> 16);
			} else {
				return original;
			}
		};

		switch(target.bank) {
			default:
				logger.error().append("Unrecognised IOC write of %02x to %08x i.e. bank %d / type %d", bus_value, address, target.bank, target.type);
			break;

			// Bank 0: internal registers.
			case 0:
				switch(target.offset) {
					default:
						logger.error().append("Unrecognised IOC bank 0 write; %02x to offset %02x", bus_value, target.offset);
					break;

					case 0x00:
						control_ = byte(bus_value);
						i2c_.set_clock_data(!(bus_value & 2), !(bus_value & 1));

						// Per the A500 documentation:
						// b7: vertical sync/test input bit, so should be programmed high;
						// b6: input for printer acknowledgement, so should be programmed high;
						// b5: speaker mute; 1 = muted;
						// b4: "Available on the auxiliary I/O connector"
						// b3: "Programmed HIGH, unless Reset Mask is required."
						// b2: Used as the floppy disk (READY) input and must be programmed high;
						// b1 and b0: I2C connections as above.
					break;

					case 0x04:
						serial_.output(IOCParty, byte(bus_value));
						irq_b_.clear(IRQB::KeyboardTransmitEmpty);
						observer_.update_interrupts();
					break;

					case 0x14:
						// b2: clear IF.
						// b3: clear IR.
						// b4: clear POR.
						// b5: clear TM[0].
						// b6: clear TM[1].
						irq_a_.clear(byte(bus_value) & 0x7c);
						observer_.update_interrupts();
					break;

					// Interrupts.
					case 0x18:
						irq_a_.mask = byte(bus_value);
//						logger.error().append("IRQ A mask set to %02x", byte(bus_value));
					break;
					case 0x28:
						irq_b_.mask = byte(bus_value);
//						logger.error().append("IRQ B mask set to %02x", byte(bus_value));
					break;
					case 0x38:
						fiq_.mask = byte(bus_value);
//						logger.error().append("FIQ mask set to %02x", byte(bus_value));
					break;

					// Counters.
					case 0x40:	case 0x50:	case 0x60:	case 0x70:
						counters_[(target.offset >> 4) - 0x4].reload = uint16_t(
							(counters_[(target.offset >> 4) - 0x4].reload & 0xff00) | byte(bus_value)
						);
					break;

					case 0x44:	case 0x54:	case 0x64:	case 0x74:
						counters_[(target.offset >> 4) - 0x4].reload = uint16_t(
							(counters_[(target.offset >> 4) - 0x4].reload & 0x00ff) | (byte(bus_value) << 8)
						);
					break;

					case 0x48:	case 0x58:	case 0x68:	case 0x78:
						counters_[(target.offset >> 4) - 0x4].value = counters_[(target.offset >> 4) - 0x4].reload;
					break;

					case 0x4c:	case 0x5c:	case 0x6c:	case 0x7c:
						counters_[(target.offset >> 4) - 0x4].output = counters_[(target.offset >> 4) - 0x4].value;
					break;
				}
			break;

			// Bank 5: both the hard disk and the latches, depending on type.
			case 5:
				switch(target.type) {
					default:
						logger.error().append("Unrecognised IOC bank 5 type %d write; %02x to offset %02x", target.type, bus_value, target.offset);
					break;

					case Address::Type::Fast:
						switch(target.offset) {
							default:
								logger.error().append("Unrecognised IOC fast bank 5 write; %02x to offset %02x", bus_value, target.offset);
							break;

							case 0x00:
								logger.error().append("TODO: printer data write; %02x", byte(bus_value));
							break;

							case 0x18:
								// TODO, per the A500 documentation:
								//
								// Latch B:
								//	b0: ?
								//	b1: double/single density; 0 = double.
								//	b2: ?
								// 	b3: floppy drive reset; 0 = reset.
								//	b4: printer strobe
								//	b5: ?
								//	b6: ?
								//	b7: HS3?

								logger.error().append("TODO: latch B write; %02x", byte(bus_value));
							break;

							case 0x40: {
								// TODO, per the A500 documentation:
								//
								// Latch A:
								//	b0, b1, b2, b3 = drive selects;
								//	b4 = side select;
								//	b5 = motor on/off
								//	b6 = floppy in use (i.e. LED?);
								//	b7 = "Not used."

								const uint8_t value = byte(bus_value);
//								logger.error().append("TODO: latch A write; %02x", value);

								// Set the floppy indicator on if any drive is selected,
								// because this emulator is compressing them all into a
								// single LED, and the machine has indicated 'in use'.
								if(activity_observer_) {
									activity_observer_->set_led_status(FloppyActivityLED,
										!(value & 0x40) && ((value & 0xf) != 0xf)
									);
								}
							} break;

							case 0x48:
								// TODO, per the A500 documentation:
								//
								// Latch C:
								//		(probably not present on earlier machines?)
								//	b2/b3: sync polarity [b3 = V polarity, b2 = H?]
								//	b0/b1: VIDC master clock; 00 = 24Mhz, 01 = 25.175Mhz; 10 = 36Mhz; 11 = reserved.

								logger.error().append("TODO: latch C write; %02x", byte(bus_value));
							break;
						}
					break;
				}
			break;
		}

//			case 0x327'0000 & AddressMask:	// Bank 7
//				logger.error().append("TODO: exteded external podule space");
//			return true;
//
//			case 0x331'0000 & AddressMask:
//				logger.error().append("TODO: 1772 / disk write");
//			return true;
//
//			case 0x336'0000 & AddressMask:
//				logger.error().append("TODO: podule interrupt request");
//			return true;
//
//			case 0x336'0004 & AddressMask:
//				logger.error().append("TODO: podule interrupt mask");
//			return true;
//
//			case 0x33a'0000 & AddressMask:
//				logger.error().append("TODO: 6854 / econet write");
//			return true;
//
//			case 0x33b'0000 & AddressMask:
//				logger.error().append("TODO: 6551 / serial line write");
//			return true;
		return true;
	}

	InputOutputController(InterruptObserverT &observer, ClockRateObserverT &clock_observer, const uint8_t *ram) :
		observer_(observer),
		keyboard_(serial_),
		sound_(*this, ram),
		video_(*this, clock_observer, sound_, ram)
	{
		irq_a_.status = IRQA::SetAlways | IRQA::PowerOnReset;
		irq_b_.status = 0x00;
		fiq_.status = FIQ::SetAlways;

		i2c_.add_peripheral(&cmos_, 0xa0);
		update_interrupts();
	}

	auto &sound() 					{	return sound_;	}
	const auto &sound() const		{	return sound_;	}
	auto &video()			 		{	return video_;	}
	const auto &video() const 		{	return video_;	}
	auto &keyboard()			 	{	return keyboard_;	}
	const auto &keyboard() const 	{	return keyboard_;	}

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

	void set_activity_observer(Activity::Observer *observer) {
		activity_observer_ = observer;
		if(activity_observer_) {
			activity_observer_->register_led(FloppyActivityLED);
		}
	}

private:
	Log::Logger<Log::Source::ARMIOC> logger;
	InterruptObserverT &observer_;
	Activity::Observer *activity_observer_ = nullptr;
	static inline const std::string FloppyActivityLED = "Drive";

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
	Video<InputOutputController, ClockRateObserverT, Sound<InputOutputController>> video_;
};

}

