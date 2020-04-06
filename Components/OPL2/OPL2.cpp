//
//  OPL2.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. all rights reserved.
//

#include "OPL2.hpp"

#include <cmath>

namespace {

/*

	Credit for the fixed register lists goes to Nuke.YKT; I found them at:
	https://siliconpr0n.org/archive/doku.php?id=vendor:yamaha:opl2#ym2413_instrument_rom

	The arrays below begin with channel 1, each line is a single channel and the
	format per channel is, from first byte to eighth:

		Bytes 1 and 2:
			Registers 1 and 2, i.e. modulator and carrier
			amplitude modulation select, vibrato select, etc.

		Byte 3:
			b7, b6: modulator key scale level
			b5...b0: modulator total level (inverted)

		Byte 4:
			b7: carrier key scale level
			b3...b0: feedback level and waveform selects as per register 4

		Bytes 5, 6:
			Registers 4 and 5, i.e. decay and attack rate, modulator and carrier.

		Bytes 7, 8:
			Registers 6 and 7, i.e. decay-sustain level and release rate, modulator and carrier.

*/

constexpr uint8_t opll_patch_set[] = {
	0x71, 0x61, 0x1e, 0x17, 0xd0, 0x78, 0x00, 0x17,
	0x13, 0x41, 0x1a, 0x0d, 0xd8, 0xf7, 0x23, 0x13,
	0x13, 0x01, 0x99, 0x00, 0xf2, 0xc4, 0x11, 0x23,
	0x31, 0x61, 0x0e, 0x07, 0xa8, 0x64, 0x70, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe0, 0x76, 0x00, 0x28,
	0x31, 0x22, 0x16, 0x05, 0xe0, 0x71, 0x00, 0x18,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x10, 0x07,
	0x23, 0x21, 0x2d, 0x14, 0xa2, 0x72, 0x00, 0x07,
	0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17,
	0x41, 0x61, 0x0b, 0x18, 0x85, 0xf7, 0x71, 0x07,
	0x13, 0x01, 0x83, 0x11, 0xfa, 0xe4, 0x10, 0x04,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x61, 0x50, 0x0c, 0x05, 0xc2, 0xf5, 0x20, 0x42,
	0x01, 0x01, 0x55, 0x03, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x41, 0x89, 0x03, 0xf1, 0xe4, 0x40, 0x13,
};

constexpr uint8_t vrc7_patch_set[] = {
	0x03, 0x21, 0x05, 0x06, 0xe8, 0x81, 0x42, 0x27,
	0x13, 0x41, 0x14, 0x0d, 0xd8, 0xf6, 0x23, 0x12,
	0x11, 0x11, 0x08, 0x08, 0xfa, 0xb2, 0x20, 0x12,
	0x31, 0x61, 0x0c, 0x07, 0xa8, 0x64, 0x61, 0x27,
	0x32, 0x21, 0x1e, 0x06, 0xe1, 0x76, 0x01, 0x28,
	0x02, 0x01, 0x06, 0x00, 0xa3, 0xe2, 0xf4, 0xf4,
	0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x11, 0x07,
	0x23, 0x21, 0x22, 0x17, 0xa2, 0x72, 0x01, 0x17,
	0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01,
	0xb5, 0x01, 0x0f, 0x0f, 0xa8, 0xa5, 0x51, 0x02,
	0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12,
	0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16,
	0x01, 0x02, 0xd3, 0x05, 0xc9, 0x95, 0x03, 0x02,
	0x61, 0x63, 0x0c, 0x00, 0x94, 0xc0, 0x33, 0xf6,
	0x21, 0x72, 0x0d, 0x00, 0xc1, 0xd5, 0x56, 0x06,
};

constexpr uint8_t percussion_patch_set[] = {
	0x01, 0x01, 0x18, 0x0f, 0xdf, 0xf8, 0x6a, 0x6d,
	0x01, 0x01, 0x00, 0x00, 0xc8, 0xd8, 0xa7, 0x48,
	0x05, 0x01, 0x00, 0x00, 0xf8, 0xaa, 0x59, 0x55,
};

}

using namespace Yamaha;

// MARK: - Construction

OPL2::OPL2(Personality personality, Concurrency::DeferringAsyncTaskQueue &task_queue): task_queue_(task_queue), personality_(personality) {
	// Populate the exponential and log-sine tables; formulas here taken from Matthew Gambrell
	// and Olli Niemitalo's decapping and reverse-engineering of the OPL2.
	for(int c = 0; c < 256; ++c) {
		exponential_[c] = int(round((pow(2.0, double(c) / 256.0) - 1.0) * 1024.0));

		const double sine = sin((double(c) + 0.5) * M_PI/512.0);
		log_sin_[c] = int(
			round(
				-log(sine) / log(2.0) * 256.0
			 )
		);
	}

	// TODO: use this when in OPLL percussion mode.
	(void)percussion_patch_set;
}

// MARK: - Audio Generation

bool OPL2::is_zero_level() {
	return true;
}

void OPL2::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// TODO.
	//  out = exp(logsin(phase2 + exp(logsin(phase1) + gain1)) + gain2)

	/*
		Melodic channels are:

		Channel		Operator 1		Operator 2
		0			0				3
		1			1				4
		2			2				5
		3			6				9
		4			7				10
		5			8				11
		6			12				15
		7			13				16
		8			14				17

		In percussion mode, only channels 0–5 are use as melodic, with 6, 7 and 8 being
		replaced by:

		Bass drum, using operators 12 and 15;
		Snare, using operator 16;
		Tom tom, using operator 14,
		Cymbal, using operator 17; and
		Symbol, using operator 13.
	*/
}

void OPL2::set_sample_volume_range(std::int16_t range) {
	// TODO.
}

// MARK: - Software Interface

void OPL2::write(uint16_t address, uint8_t value) {
	if(address & 1) {
		switch(personality_) {
			case Personality::OPL2:
				set_opl2_register(selected_register_, value);
			break;
			default:
				set_opll_register(selected_register_, value);
			break;
		}
	} else {
		selected_register_ = value;
	}
}

uint8_t OPL2::read(uint16_t address) {
	// TODO. There's a status register where:
	//	b7 = IRQ status (set if interrupt request ongoing)
	//	b6 = timer 1 flag (set if timer 1 expired)
	//	b5 = timer 2 flag
	return 0xff;
}

void OPL2::set_opll_register(uint8_t location, uint8_t value) {
	if(location < 8) {
		opll_custom_instrument_[location] = value;
		// Repush this instrument for any channels it's presently selected on.
		for(int c = 0; c < 9; ++c) {
			if(!(instrument_selections_[c] >> 4)) {
				set_opll_instrument(uint8_t(c), 0, instrument_selections_[c] & 0xf);
			}
		}
		return;
	}

	if(location >= 0x30 && location <= 0x38) {
		instrument_selections_[location - 0x30] = value;
		set_opll_instrument(location - 0x30, value >> 4, value & 0xf);
		return;
	}

	if(location == 0xe) {
		set_opl2_register(0xbd, value & 0x3f);
		return;
	}

	if(location >= 0x10 && location <= 0x18) {
		set_opl2_register(location - 0x10 + 0xa0, value);
		return;
	}

	if(location >= 0x20 && location <= 0x28) {
		const auto index = location = 0x20;
		operators_[index].hold_sustain_level = value & 0x20;

		// Only the bottom bit contributes to the frequency on an OPLL; on an OPL2 it's the two
		// bottom bits (and hold-sustain isn't set in the same register).
		set_opl2_register(index + 0xb0, uint8_t((value & 1) | ((value & 0xfe) << 1)));
		return;
	}
}

void OPL2::set_opll_instrument(uint8_t target, uint8_t source_instrument, uint8_t volume) {
	const uint8_t *source;
	if(!source_instrument) {
		source = opll_custom_instrument_;
	} else {
		--source_instrument;
		source =
			(source_instrument * 8) +
			(personality_ == Personality::OPLL ? opll_patch_set : vrc7_patch_set);
	}

	constexpr uint8_t offsets[9][2] = {
		{0x00, 0x03},
		{0x01, 0x04},
		{0x02, 0x05},

		{0x08, 0x0b},
		{0x09, 0x0c},
		{0x0a, 0x0d},

		{0x10, 0x13},
		{0x11, 0x14},
		{0x12, 0x15},
	};
	const auto carrier = offsets[target][0];
	const auto modulator = offsets[target][1];

	// Set waveforms — only sine and halfsine are available.
	set_opl2_register(0xe0 + carrier, (source[3] & 0x10) ? 1 : 0);
	set_opl2_register(0xe0 + modulator, (source[3] & 0x08) ? 1 : 0);

	// Volume on the OPLL is four bit; on the OPL2 it's six. Pair that with key scale level.
	set_opl2_register(0xe0 + carrier, uint8_t((source[3] & 0xc0) | (volume << 2)));

	// Set feedback level, which is per channel. And always set frequency modulation.
	set_opl2_register(0xc0 + target, uint8_t((source[3] & 0x7) << 1));

	// The other values don't require any mapping.
	set_opl2_register(0x20 + carrier, source[0]);
	set_opl2_register(0x20 + modulator, source[1]);
	set_opl2_register(0x40 + carrier, source[2]);
	set_opl2_register(0x60 + carrier, source[4]);
	set_opl2_register(0x60 + modulator, source[5]);
	set_opl2_register(0x80 + carrier, source[6]);
	set_opl2_register(0x80 + modulator, source[7]);
}

void OPL2::set_opl2_register(uint8_t location, uint8_t value) {
	printf("OPL2 write: %02x to %d\n", value, selected_register_);

	// Deal with timer changes synchronously.
	switch(location) {
		case 0x02:	timers_[0] = value; 	return;
		case 0x03:	timers_[1] = value;		return;
		case 0x04:	timer_control_ = value;	return;
		// TODO from register 4:
		//	b7 = IRQ reset;
		//	b6/b5 = timer 1/2 mask (irq enabling flags, I think?)
		//	b4/b3 = timer 2/1 start (seemingly the opposite order to b6/b5?)

		default: break;
	}

	// Enqueue any changes that affect audio output.
	task_queue_.enqueue([this, location, value] {

		//
		// Operator modifications.
		//

		// The 18 operators are spreat out across 22 addresses; each group of
		// six is framed within an eight-byte area thusly:
		constexpr int operator_by_address[] = {
			0,	1,	2,	3,	4,	5,	-1,	-1,
			6,	7,	8,	9,	10,	11,	-1,	-1,
			12,	13,	14,	15,	16,	17,	-1,	-1
		};

		if(location >= 0x20 && location <= 0x35) {
			const auto index = operator_by_address[location - 0x20];
			if(index == -1) return;

			operators_[index].apply_amplitude_modulation = value & 0x80;
			operators_[index].apply_vibrato = value & 0x40;
			operators_[index].hold_sustain_level = value & 0x20;
			operators_[index].keyboard_scaling_rate = value & 0x10;
			operators_[index].frequency_multiple = value & 0xf;
			return;
		}

		if(location >= 0x40 && location <= 0x55) {
			const auto index = operator_by_address[location - 0x40];
			if(index == -1) return;

			operators_[index].scaling_level = value >> 6;
			operators_[index].output_level = value & 0x3f;
			return;
		}

		if(location >= 0x60 && location <= 0x75) {
			const auto index = operator_by_address[location - 0x60];
			if(index == -1) return;

			operators_[index].attack_rate = value >> 5;
			operators_[index].decay_rate = value & 0xf;
			return;
		}

		if(location >= 0x80 && location <= 0x95) {
			const auto index = operator_by_address[location - 0x80];
			if(index == -1) return;

			operators_[index].sustain_level = value >> 5;
			operators_[index].release_rate = value & 0xf;
			return;
		}

		if(location >= 0xe0 && location <= 0xf5) {
			const auto index = operator_by_address[location - 0xe0];
			if(index == -1) return;

			operators_[index].waveform = value & 3;
			return;
		}


		//
		// Channel modifications.
		//

		if(location >= 0xa0 && location <= 0xa8) {
			channels_[location - 0xa0].frequency = (channels_[location - 0xa0].frequency & ~0xff) | value;
			return;
		}

		if(location >= 0xb0 && location <= 0xb8) {
			channels_[location - 0xb0].frequency = (channels_[location - 0xb0].frequency & 0xff) | ((value & 3) << 8);
			channels_[location - 0xb0].octave = (value >> 2) & 0x7;
			channels_[location - 0xb0].key_on = value & 0x20;;
			return;
		}

		if(location >= 0xc0 && location <= 0xc8) {
			channels_[location - 0xc0].feedback_strength = (value >> 1) & 0x7;
			channels_[location - 0xc0].use_fm_synthesis = value & 1;
			return;
		}


		//
		// Modal modifications.
		//

		switch(location) {
			case 0x01:	waveform_enable_ = value & 0x20;	break;
			case 0x08:
				// b7: "composite sine wave mode on/off"?
				csm_keyboard_split_ = value;
				// b6: "Controls the split point of the keyboard. When 0, the keyboard split is the
				// second bit from the bit 8 of the F-Number. When 1, the MSB of the F-Number is used."
			break;
			case 0xbd:	depth_rhythm_control_ = value;		break;

			default: break;
		}
	});
}
