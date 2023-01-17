//
//  RP5C01.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "RP5C01.hpp"

using namespace Ricoh::RP5C01;

RP5C01::RP5C01(HalfCycles clock_rate) : clock_rate_(clock_rate) {}

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
			++year_;
			leap_year_ = (leap_year_ + 1) & 3;
		}
	}
}

/// Performs a write of @c value to @c address.
void RP5C01::write(int address, uint8_t value) {
	address &= 0xf;

	// Registers D–F don't depend on the mode.
	if(address >= 0xd) {
		switch(address) {
			default: break;
			case 0xd:
				timer_enabled_ = value & 0x8;
				alarm_enabled_ = value & 0x4;
				mode_ = value & 0x3;
			break;
			case 0xe:
				// Test register; unclear what is supposed to happen.
			break;
			case 0xf:
				one_hz_on_ = !(value & 0x8);
				sixteen_hz_on_ = !(value & 0x4);
				// TODO: timer reset on bit 1, alarm reset on bit 0
			break;
		}

		return;
	}

	switch(mode_) {
		case 3:
			address += 13;
			[[fallthrough]];
		case 2:
			ram_[size_t(address)] = value & 0xf;
		return;
	}

	// TODO.
	printf("RP-5C01 write of %d to %d in mode %d\n", value, address & 0xf, mode_);
}

uint8_t RP5C01::read(int address) {
	address &= 0xf;

	if(address < 0xd) {
		switch(mode_) {
			case 3:
				address += 13;
				[[fallthrough]];
			case 2:
			return 0xf0 | ram_[size_t(address)];
		}
	}

	// TODO.
	printf("RP-5C01 read from %d in mode %d\n", address & 0xf, mode_);
	return 0xff;
}
