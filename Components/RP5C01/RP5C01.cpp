//
//  RP5C01.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "RP5C01.hpp"

#include <ctime>

using namespace Ricoh::RP5C01;

RP5C01::RP5C01(HalfCycles clock_rate) : clock_rate_(clock_rate) {
	// Seed internal clock.
	std::time_t now = std::time(NULL);
	std::tm *time_date = std::localtime(&now);

	seconds_ =
		time_date->tm_sec +
		time_date->tm_min * 60 +
		time_date->tm_hour * 60 * 60;

	day_of_the_week_ = time_date->tm_wday;
	day_ = time_date->tm_mday;
	month_ = time_date->tm_mon;
	year_ = time_date->tm_year % 100;
	leap_year_ = time_date->tm_year % 4;
}

void RP5C01::run_for(HalfCycles cycles) {
	sub_seconds_ += cycles;

	// Guess: this happens so rarely (i.e. once a second, ordinarily) that
	// it's not worth worrying about the branch prediction consequences.
	//
	// ... and ditto all the conditionals below, which will be very rarely reached.
	if(sub_seconds_ < clock_rate_) {
		return;
	}
	const auto elapsed_seconds = int(sub_seconds_.as_integral() / clock_rate_.as_integral());
	sub_seconds_ %= clock_rate_;

	// Update time within day.
	seconds_ += elapsed_seconds;

	constexpr int day_length = 60 * 60 * 24;
	if(seconds_ < day_length) {
		return;
	}
	const int elapsed_days = seconds_ / day_length;
	seconds_ %= day_length;

	// Day of the week doesn't aggregate upwards.
	day_of_the_week_ = (day_of_the_week_ + elapsed_days) % 7;

	// Assumed for now: day and month run from 0.
	// A leap year count of 0 implies a leap year.
	// TODO: verify.
	day_ += elapsed_days;
	while(true) {
		int month_length = 1;
		switch(month_) {
			case 0:	month_length = 31;					break;
			case 1:	month_length = 28 + !leap_year_;	break;
			case 2: month_length = 31;					break;
			case 3: month_length = 30;					break;
			case 4: month_length = 31;					break;
			case 5: month_length = 30;					break;
			case 6: month_length = 31;					break;
			case 7: month_length = 31;					break;
			case 8: month_length = 30;					break;
			case 9: month_length = 31;					break;
			case 10: month_length = 30;					break;
			case 11: month_length = 31;					break;
		}

		if(day_ < month_length) {
			return;
		}

		day_ -= month_length;
		++month_;

		if(month_ == 12) {
			month_ = 0;
			year_ = (year_ + 1) % 100;
			leap_year_ = (leap_year_ + 1) & 3;
		}
	}
}

namespace {

constexpr int Reg(int mode, int address) {
	return address | mode << 4;
}

}

/// Performs a write of @c value to @c address.
void RP5C01::write(int address, uint8_t value) {
	address &= 0xf;
	value &= 0xf;

	// Handle potential RAM accesses.
	if(address < 0xd && mode_ >= 2) {
		address += mode_ == 3 ? 13 : 0;
		ram_[size_t(address)] = value & 0xf;
		return;
	}

	switch(Reg(mode_, address)) {
		default: break;

		// Seconds.
		case Reg(0, 0x00):
			seconds_ = seconds_ - (seconds_ % 10) + (value % 10);
		break;
		case Reg(0, 0x01):
			seconds_ = (seconds_ % 10) + ((value % 6) * 10);
		break;

		// TODO: minutes.
		case Reg(0, 0x02):
		case Reg(0, 0x03):	break;

		// TODO: hours.
		case Reg(0, 0x04):
		case Reg(0, 0x05):	break;

		// TODO: day-of-the-week counter.
		case Reg(0, 0x06):	break;

		// TODO: day counter.
		case Reg(0, 0x07):
		case Reg(0, 0x08):	break;

		// TODO: month counter.
		case Reg(0, 0x09):
		case Reg(0, 0x0a):	break;

		// TODO: year counter.
		case Reg(0, 0x0b):
		case Reg(0, 0x0c):	break;

		// TODO: alarm minutes.
		case Reg(1, 0x02):
		case Reg(1, 0x03):	break;

		// TODO: alarm hours.
		case Reg(1, 0x04):
		case Reg(1, 0x05):	break;

		// TODO: alarm day-of-the-week.
		case Reg(1, 0x06):	break;

		// TODO: alarm day.
		case Reg(1, 0x07):
		case Reg(1, 0x08):	break;

		case Reg(1, 0x0a):
			twentyfour_hour_clock_ = value & 1;
		break;

		// TODO: leap-year counter.
		case Reg(1, 0x0b):	break;

		//
		// Registers D–F don't depend on the mode.
		//

		case Reg(0, 0xd):	case Reg(1, 0xd):	case Reg(2, 0xd):	case Reg(3, 0xd):
			timer_enabled_ = value & 0x8;
			alarm_enabled_ = value & 0x4;
			mode_ = value & 0x3;
		break;
		case Reg(0, 0xe):	case Reg(1, 0xe):	case Reg(2, 0xe):	case Reg(3, 0xe):
			// Test register; unclear what is supposed to happen.
		break;
		case Reg(0, 0xf):	case Reg(1, 0xf):	case Reg(2, 0xf):	case Reg(3, 0xf):
			one_hz_on_ = !(value & 0x8);
			sixteen_hz_on_ = !(value & 0x4);
			// TODO: b0 = alarm reset; b1 = timer reset.
		break;
	}

}

uint8_t RP5C01::read(int address) {
	address &= 0xf;

	if(address < 0xd && mode_ >= 2) {
		address += mode_ == 3 ? 13 : 0;
		return 0xf0 | ram_[size_t(address)];
	}

	int value = 0xf;
	switch(Reg(mode_, address)) {
		// Second.
		case Reg(0, 0x00):	value = seconds_ % 10;			break;
		case Reg(0, 0x01):	value = (seconds_ / 10) % 6;	break;

		// Minute.
		case Reg(0, 0x02):	value = (seconds_ / 60) % 10;	break;
		case Reg(0, 0x03):	value = (seconds_ / 600) % 6;	break;

		// Hour.
		case Reg(0, 0x04):
			if(twentyfour_hour_clock_) {
				value = (seconds_ / 3600) % 10;
			} else {
				value = ((seconds_ / 3600) % 12) % 10;
			}
		break;
		case Reg(0, 0x05):
			if(twentyfour_hour_clock_) {
				value = (seconds_ / 36000);
			} else {
				value = ((seconds_ / 3600) / 12) + (seconds_ >= 12*60*60 ? 2 : 0);
			}
		break;

		// Day-of-the-week.
		case Reg(0, 0x06):	value = day_of_the_week_;	break;

		// Day.
		case Reg(0, 0x07):	value = day_ % 10;			break;
		case Reg(0, 0x08):	value = day_ / 10;			break;

		// Month.
		case Reg(0, 0x09):	value = month_ % 10;		break;
		case Reg(0, 0x0a):	value = month_ / 10;		break;

		// Year;
		case Reg(0, 0x0b):	value = year_ % 10;			break;
		case Reg(0, 0x0c):	value = year_ / 10;			break;

		// TODO: alarm minutes.
		case Reg(1, 0x02):
		case Reg(1, 0x03):	break;

		// TODO: alarm hours.
		case Reg(1, 0x04):
		case Reg(1, 0x05):	break;

		// TODO: alarm day-of-the-week.
		case Reg(1, 0x06):	break;

		// TODO: alarm day.
		case Reg(1, 0x07):
		case Reg(1, 0x08):	break;

		// 12/24-hour clock.
		case Reg(1, 0x0a):	value = twentyfour_hour_clock_;	break;

		// Leap year.
		case Reg(1, 0x0b):	value = leap_year_;				break;

		//
		// Registers D–F don't depend on the mode.
		//

		case Reg(0, 0xd):	case Reg(1, 0xd):	case Reg(2, 0xd):	case Reg(3, 0xd):
			value =
				(timer_enabled_ ? 0x8 : 0x0) |
				(alarm_enabled_ ? 0x4 : 0x0) |
				mode_;
		break;
		case Reg(0, 0xe):	case Reg(1, 0xe):	case Reg(2, 0xe):	case Reg(3, 0xe):
			// Test register; unclear what is supposed to happen.
		break;
		case Reg(0, 0xf):	case Reg(1, 0xf):	case Reg(2, 0xf):	case Reg(3, 0xf):
			value =
				(one_hz_on_ ? 0x0 : 0x8) |
				(sixteen_hz_on_ ? 0x0 : 0x4);
		break;
	}

	return uint8_t(0xf0 | value);
}
