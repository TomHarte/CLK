//
//  RTC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <ctime>

extern bool should_log;

namespace PCCompatible {

/*!
	Implements enough of the MC146818 to satisfy those BIOSes I've tested.
*/
class RTC {
public:
	template <int address>
	requires(address >= 0 && address < 2)
	void write(const uint8_t value) {
		switch(address) {
			default: break;
			case 0:
				selected_ = value & 0x7f;
				// NMI enable/disable not yet supported.
			break;
			case 1:
				write_register(value);
			break;
		}
	}

	uint8_t read() const {
		const auto time_date = [] {
			const std::time_t now = std::time(NULL);
			return std::localtime(&now);
		};

		switch(selected_) {
			default:
				if(ram_selected()) {
//					printf("RTC: %02x <- %zu\n", ram_[ram_address()], ram_address());
//					if(ram_address() == 1 && ram_[1] == 6) {	// Catch reset after passing protected mode test.
//						should_log = true;
//					}
					return ram_[ram_address()];
				}
			return 0xff;

			case 0x00:	return bcd(time_date()->tm_sec);			// Seconds [0-59]
			case 0x01:	return 0;	// Seconds alarm
			case 0x02:	return bcd(time_date()->tm_min);			// Minutes [0-59]
			case 0x03:	return 0;	// Minutes alarm
			case 0x04: {
				const auto hour = time_date()->tm_hour;

				// Hours [1-12 or 0-23]
				if(is_24hour()) {
					return bcd(hour);
				}
				return
					((hour >= 12) ? 0x80 : 0x00) |
					bcd(1 + (hour + 11)%12);
			} break;
			case 0x05:	return 0;	// Hours alarm
			case 0x06:	return bcd(time_date()->tm_wday + 1);			// Day of the week [Sunday = 1]
			case 0x07:	return bcd(time_date()->tm_mday);				// Date of the Month [1-31]
			case 0x08:	return bcd(time_date()->tm_mon + 1);			// Month [1-12]
			case 0x09:	return bcd(time_date()->tm_year % 100);			// Year [0-99]
			case 0x32:	return bcd(19 + time_date()->tm_year / 100);	// Century

			case 0x0a:	return statusA_ & 0x7f;		// Exclude the update-in-progress bit.
			case 0x0b:	return statusB_;
		}
	}

private:
	std::size_t selected_;
	std::array<uint8_t, 50> ram_{};

	uint8_t statusA_ = 0x00;
	uint8_t statusB_ = 0x02;

	// Status A.
	// b7: update-in-progress.
	// b6–b4: selects condition of the divider chain (?);
	// b3–b0: selects rate of the divider chain.

	// Status B.
	bool disable_updates() const				{ return statusB_ & 0x80; }
	bool periodic_interrupt_enabled() const		{ return statusB_ & 0x40; }
	bool alarm_interrupt_enabled() const		{ return statusB_ & 0x20; }
	bool update_ended_interrupt_enabled() const	{ return statusB_ & 0x10; }
	bool square_wave_enabled() const			{ return statusB_ & 0x08; }
	bool is_decimal() const						{ return statusB_ & 0x04; }
	bool is_24hour() const						{ return statusB_ & 0x02; }
	bool daylight_savings_enabled() const		{ return statusB_ & 0x01; }

	// Helpers for differentiating RAM accesses from the more meaningful registers.
	bool ram_selected() const { return selected_ >= 0xe && selected_ < 0xe + ram_.size(); }
	std::size_t ram_address() const { return selected_ - 0xe; }

	/// Converts @c input to BCD if BCD mode is enabled; otherwise returns @c input unaltered.
	template <typename IntT>
	uint8_t bcd(const IntT input) const {
		// If calendar is in binary format, don't convert.
		if(is_decimal()) {
			return uint8_t(input);
		}

		// Convert a one or two digit number to BCD.
		return uint8_t(
			(input % 10) +
			((input / 10) * 16)
		);
	}

	/// Writes @c value to the register @c selected_ .
	void write_register(const uint8_t value) {
		switch(selected_) {
			default:
				if(ram_selected()) {
					ram_[ram_address()] = value;
					printf("RTC: %02x -> %zu\n", value, ram_address());
				}
			break;
			case 0x0a:	statusA_ = value;	break;
			case 0x0b:
				statusB_ = value;
				break;
		}
	}
};

}
