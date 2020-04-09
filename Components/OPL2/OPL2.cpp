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

using namespace Yamaha::OPL;


template <typename Child>
OPLBase<Child>::OPLBase(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {
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
}

template <typename Child>
void OPLBase<Child>::write(uint16_t address, uint8_t value) {
	if(address & 1) {
		static_cast<Child *>(this)->write_register(selected_register_, value);
	} else {
		selected_register_ = value;
	}
}

template class Yamaha::OPL::OPLBase<Yamaha::OPL::OPLL>;
template class Yamaha::OPL::OPLBase<Yamaha::OPL::OPL2>;


OPLL::OPLL(Concurrency::DeferringAsyncTaskQueue &task_queue, bool is_vrc7): OPLBase(task_queue) {
	// Install fixed instruments.
	const uint8_t *patch_set = is_vrc7 ? vrc7_patch_set : opll_patch_set;
	for(int c = 0; c < 15; ++c) {
		setup_fixed_instrument(c+1, patch_set);
		patch_set += 8;
	}

	// TODO: install percussion.
	(void)percussion_patch_set;
}

bool OPLL::is_zero_level() {
	return true;
}

void OPLL::get_samples(std::size_t number_of_samples, std::int16_t *target) {
}

void OPLL::set_sample_volume_range(std::int16_t range) {
}

uint8_t OPLL::read(uint16_t address) {
	// I've seen mention of an undocumented two-bit status register. I don't yet know what is in it.
	return 0xff;
}

void OPLL::write_register(uint8_t address, uint8_t value) {
	// The OPLL doesn't have timers or other non-audio functions, so all writes
	// go to the audio queue.
	task_queue_.defer([this, address, value] {
		// The first 8 locations are used to define the custom instrument, and have
		// exactly the same format as the patch set arrays at the head of this file.
		if(address < 8) {
			custom_instrument_[address] = value;

			// Update whatever that did to the instrument.
			setup_fixed_instrument(0, custom_instrument_);
			return;
		}

		// Locations 0x30 to 0x38: select an instrument in the top nibble, set a channel volume in the lower.
		if(address >= 0x30 && address <= 0x38) {
			const auto index = address - 0x30;
			const auto instrument = value >> 4;

			channels_[index].output_level = value & 0xf;
			channels_[index].modulator = &operators_[instrument * 2];
			return;
		}

		// Register 0xe is a cut-down version of the OPLL's register 0xbd.
		if(address == 0xe) {
			depth_rhythm_control_ = value & 0x3f;
			return;
		}

		// Registers 0x10 to 0x18 set the bottom part of the channel frequency.
		if(address >= 0x10 && address <= 0x18) {
			const auto index = address - 0x10;
			channels_[index].frequency = (channels_[index].frequency & ~0xff) | value;
			return;
		}

		// 0x20 to 0x28 set sustain on/off, key on/off, octave and a single extra bit of frequency.
		// So they're a lot like OPLL registers 0xb0 to 0xb8, but not identical.
		if(address >= 0x20 && address <= 0x28) {
			const auto index = address - 0x20;
			channels_[index].frequency = (channels_[index].frequency & 0xff) | (value & 1);
			channels_[index].octave = (value >> 1) & 0x7;
			channels_[index].key_on = value & 0x10;
			channels_[index].hold_sustain_level = value & 0x20;
			return;
		}
	});
}

void OPLL::setup_fixed_instrument(int number, const uint8_t *data) {
	auto modulator = &operators_[number * 2];
	auto carrier = &operators_[number * 2 + 1];

	// Set waveforms — only sine and halfsine are available.
	carrier->waveform = Operator::Waveform((data[3] & 0x10) ? 1 : 0);
	modulator->waveform = Operator::Waveform((data[3] & 0x08) ? 1 : 0);

	// Set modulator amplitude and key-scale level.
	modulator->scaling_level = data[2] >> 6;
	modulator->output_level = data[2] & 0x3f;

//	set_opl2_register(0x20 + carrier, source[0]);
//	set_opl2_register(0x20 + modulator, source[1]);
//	set_opl2_register(0x40 + carrier, source[2]);
//	set_opl2_register(0x60 + carrier, source[4]);
//	set_opl2_register(0x60 + modulator, source[5]);
//	set_opl2_register(0x80 + carrier, source[6]);
//	set_opl2_register(0x80 + modulator, source[7]);
}

/*
template <Personality personality>
void OPL2<personality>::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// TODO.
	//  out = exp(logsin(phase2 + exp(logsin(phase1) + gain1)) + gain2)

//		Melodic channels are:
//
//		Channel		Operator 1		Operator 2
//		0			0				3
//		1			1				4
//		2			2				5
//		3			6				9
//		4			7				10
//		5			8				11
//		6			12				15
//		7			13				16
//		8			14				17
//
//		In percussion mode, only channels 0–5 are use as melodic, with 6, 7 and 8 being
//		replaced by:
//
//		Bass drum, using operators 12 and 15;
//		Snare, using operator 16;
//		Tom tom, using operator 14,
//		Cymbal, using operator 17; and
//		Symbol, using operator 13.
}

*/

void OPL2::write_register(uint8_t address, uint8_t value) {

	// Deal with timer changes synchronously.
	switch(address) {
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
	task_queue_.enqueue([this, address, value] {
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

		if(address >= 0x20 && address <= 0x35) {
			const auto index = operator_by_address[address - 0x20];
			if(index == -1) return;

			operators_[index].apply_amplitude_modulation = value & 0x80;
			operators_[index].apply_vibrato = value & 0x40;
			operators_[index].hold_sustain_level = value & 0x20;
			operators_[index].keyboard_scaling_rate = value & 0x10;
			operators_[index].frequency_multiple = value & 0xf;
			return;
		}

		if(address >= 0x40 && address <= 0x55) {
			const auto index = operator_by_address[address - 0x40];
			if(index == -1) return;

			operators_[index].scaling_level = value >> 6;
			operators_[index].output_level = value & 0x3f;
			return;
		}

		if(address >= 0x60 && address <= 0x75) {
			const auto index = operator_by_address[address - 0x60];
			if(index == -1) return;

			operators_[index].attack_rate = value >> 5;
			operators_[index].decay_rate = value & 0xf;
			return;
		}

		if(address >= 0x80 && address <= 0x95) {
			const auto index = operator_by_address[address - 0x80];
			if(index == -1) return;

			operators_[index].sustain_level = value >> 5;
			operators_[index].release_rate = value & 0xf;
			return;
		}

		if(address >= 0xe0 && address <= 0xf5) {
			const auto index = operator_by_address[address - 0xe0];
			if(index == -1) return;

			operators_[index].waveform = Operator::Waveform(value & 3);
			return;
		}


		//
		// Channel modifications.
		//

		if(address >= 0xa0 && address <= 0xa8) {
			channels_[address - 0xa0].frequency = (channels_[address - 0xa0].frequency & ~0xff) | value;
			return;
		}

		if(address >= 0xb0 && address <= 0xb8) {
			channels_[address - 0xb0].frequency = (channels_[address - 0xb0].frequency & 0xff) | ((value & 3) << 8);
			channels_[address - 0xb0].octave = (value >> 2) & 0x7;
			channels_[address - 0xb0].key_on = value & 0x20;;
			return;
		}

		if(address >= 0xc0 && address <= 0xc8) {
			channels_[address - 0xc0].feedback_strength = (value >> 1) & 0x7;
			channels_[address - 0xc0].use_fm_synthesis = value & 1;
			return;
		}


		//
		// Modal modifications.
		//

		switch(address) {
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

uint8_t OPL2::read(uint16_t address) {
	// TODO. There's a status register where:
	//	b7 = IRQ status (set if interrupt request ongoing)
	//	b6 = timer 1 flag (set if timer 1 expired)
	//	b5 = timer 2 flag
	return 0xff;
}
