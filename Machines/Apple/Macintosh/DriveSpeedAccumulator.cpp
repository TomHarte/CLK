//
//  DriveSpeedAccumulator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DriveSpeedAccumulator.hpp"

namespace {

/*
	For knowledge encapsulate below, all credit goes to the MAME team. No original research here.

	Per their investigation, the bytes collected for PWM output feed a 6-bit LFSR, which then keeps
	output high until it eventually reaches a state of 0x20. The LFSR shifts rightward and taps bits
	0 and 1 as the new input into bit 5.

	I've therefore implemented the LFSR as below, feeding into a lookup table to calculate actual
	pulse widths from the values stored into the PWM buffer.
*/
template<uint8_t value> constexpr uint8_t lfsr() {
        if constexpr (value == 0x20 || !value) return 0;
        return 1+lfsr<(((value ^ (value >> 1))&1) << 5) | (value >> 1)>();
}

constexpr uint8_t pwm_lookup[] = {
        lfsr<0>(),        lfsr<1>(),        lfsr<2>(),        lfsr<3>(),        lfsr<4>(),        lfsr<5>(),        lfsr<6>(),        lfsr<7>(),
        lfsr<8>(),        lfsr<9>(),        lfsr<10>(),       lfsr<11>(),       lfsr<12>(),       lfsr<13>(),       lfsr<14>(),       lfsr<15>(),
        lfsr<16>(),       lfsr<17>(),       lfsr<18>(),       lfsr<19>(),       lfsr<20>(),       lfsr<21>(),       lfsr<22>(),       lfsr<23>(),
        lfsr<24>(),       lfsr<25>(),       lfsr<26>(),       lfsr<27>(),       lfsr<28>(),       lfsr<29>(),       lfsr<30>(),       lfsr<31>(),
        lfsr<32>(),       lfsr<33>(),       lfsr<34>(),       lfsr<35>(),       lfsr<36>(),       lfsr<37>(),       lfsr<38>(),       lfsr<39>(),
        lfsr<40>(),       lfsr<41>(),       lfsr<42>(),       lfsr<43>(),       lfsr<44>(),       lfsr<45>(),       lfsr<46>(),       lfsr<47>(),
        lfsr<48>(),       lfsr<49>(),       lfsr<50>(),       lfsr<51>(),       lfsr<52>(),       lfsr<53>(),       lfsr<54>(),       lfsr<55>(),
        lfsr<56>(),       lfsr<57>(),       lfsr<58>(),       lfsr<59>(),       lfsr<60>(),       lfsr<61>(),       lfsr<62>(),       lfsr<63>(),
};

}

using namespace Apple::Macintosh;

void DriveSpeedAccumulator::post_sample(uint8_t sample) {
	if(!delegate_) return;

	// An Euler-esque approximation is used here: just collect all
	// the samples until there is a certain small quantity of them,
	// then produce a new estimate of rotation speed and start the
	// buffer afresh.
	//
	// Note the table lookup here; see text above.
	sample_total_ += pwm_lookup[sample & 0x3f];
	++sample_count_;

	if(sample_count_ == samples_per_bucket) {
		// The below fits for a function like `a + bc`; it encapsultes the following
		// beliefs:
		//
		//	(i) motor speed is proportional to voltage supplied;
		//	(ii) with pulse-width modulation it's therefore proportional to the duty cycle;
		//	(iii) the Mac pulse-width modulates whatever it reads from the disk speed buffer, as per the LFSR rules above;
		//	(iv) ... subject to software pulse-width modulation of that pulse-width modulation.
		//
		// So, I believe current motor speed is proportional to a low-pass filtering of
		// the speed buffer. Which I've implemented very coarsely via 'large' bucketed-averages,
		// noting also that exact disk motor speed is always a little approximate.

		// The formula below was derived from observing values the Mac wrote into its
		// disk-speed buffer. Given that it runs a calibration loop before doing so,
		// I cannot guarantee the accuracy of these numbers beyond being within the
		// range that the computer would accept.
		const float normalised_sum = float(sample_total_) / float(samples_per_bucket);
		const float rotation_speed = (normalised_sum - 3.7f) * 17.6f;

		delegate_->drive_speed_accumulator_set_drive_speed(this, rotation_speed);
		sample_count_ = 0;
		sample_total_ = 0;
	}
}
