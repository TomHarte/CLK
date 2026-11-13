//
//  Sound.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Sound.hpp"

#include <cassert>
#include <cstdio>
#include <numeric>

namespace {

struct GLUControl {
	static constexpr uint8_t Volume					= 0b0000'1111;
	static constexpr uint8_t Reserved				= 0b0001'0000;
	static constexpr uint8_t AddressAutoIncrement	= 0b0010'0000;
	static constexpr uint8_t AccessRAM				= 0b0100'0000;
	static constexpr uint8_t DOCBusy				= 0b1000'0000;
};

struct DOCInterrupt {
	static constexpr uint8_t NoInterrupt			= 0b1000'0000;
	static constexpr uint8_t Reserved				= 0b0100'0001;
	static constexpr uint8_t Oscillator				= 0b0011'1110;

	static constexpr uint8_t interrupt(const uint8_t oscillator) {
		return uint8_t((oscillator << 1) | Reserved);
	}
	static constexpr uint8_t no_interrupt() {
		return NoInterrupt | Reserved;
	}
};

struct OscillatorControl {
	static constexpr uint8_t Halted					= 0b0000'0001;
	static constexpr uint8_t InterruptEnabled		= 0b0000'1000;
	static constexpr uint8_t ChannelAddress			= 0b1111'0000;

	enum class Mode {
		FreeRun = 0b00,
		OneShot = 0b01,
		Sync = 0b10,
		Swap = 0b11,
	};
	static constexpr Mode mode(const uint8_t control) {
		return Mode((control >> 1) & 3);
	}
	static constexpr uint8_t channel_address(const uint8_t control) {
		return control >> 4;
	}
};

struct WavetableSize {
	static constexpr uint8_t address_bus_resolution(const uint8_t size) {
		return size & 0b000'111;
	}
	static constexpr uint16_t table_size(const uint8_t size) {
		const auto size_field = (size & 0b111'000) >> 3;
		return uint16_t(256 << size_field);
	}
};

void load_high(uint16_t &target, const uint8_t source) {
	target &= 0x00ff;
	target |= uint16_t(source << 8);
}

void load_low(uint16_t &target, const uint8_t source) {
	target &= 0xff00;
	target |= source;
}

}

using namespace Apple::IIgs::Sound;

GLU::GLU([[maybe_unused]] float clock_rate) noexcept {}

void GLU::set_data(const uint8_t data) {
	data_ = data;
	if(control_ & GLUControl::AccessRAM) {
		ram_[address_] = data;
	} else {
		set_register(address_, data);
	}

	if(control_ & GLUControl::AddressAutoIncrement) {
		++address_;
	}
}

void GLU::set_register(const uint16_t address, const uint8_t value) {
	auto &oscillator = oscillators_[address_ & 0b0001'1111];
	switch(address_ & 0b1110'0000) {
		case 0x00:	load_low(oscillator.velocity, value);		break;
		case 0x20:	load_high(oscillator.velocity, value);		break;
		case 0x40:	oscillator.volume = value;					break;
		case 0x80:	oscillator.address = value;					break;

		case 0x60:
			/* This register is read-only. */
		break;
		case 0xa0: {
			oscillator.control = value;

			// Halt + M0 => reset position.
			if((oscillator.control & 0x3) == 3) {
				oscillator.control |= 1;
			}
		} break;
		case 0xc0:
			oscillator.table_size = value;

			// The most-significant bit that should be used is 16 + (value & 7).
			oscillator.overflow_mask = uint32_t(~(0xffffff >> (7 - (value & 7))));
		break;

		default:
			switch(address & 0xff) {
				case 0xe0:
					/* Does setting the interrupt register really make any sense? */
				break;
				case 0xe1:
					oscillator_count_ = 1 | ((value >> 1) & 31);
				break;
				case 0xe2:
					/* Writing to the analogue to digital input definitely makes no sense. */
				break;
			}
		break;
	}
}

uint8_t GLU::data() {
	// Per the Apple IIgs Hardware Reference, page 101:
	//
	//	"The sound RAM read operation is the same as the write operation with one exception: reading from the data
	//	register lags by one read cycle. For example, if you want to read 10 bytes from the sound RAM, select the RAM by
	//	setting the Sound Control register bit and enabling auto-incrementing. Then set the Address Pointer register to
	//	the startig address and read the data register 11 times, discarding the first byte read."
	//
	// Hence the code below returns whatever is currently in data_ but enqueues the proper value for it for next
	// time around.
	const auto data = data_;

	if(control_ & GLUControl::AccessRAM) {
		data_ = ram_[address_];
	} else {
		auto &oscillator = oscillators_[address_ & 0b0001'1111];
		switch(address_ & 0b1110'0000) {
			case 0x00:	data_ = oscillator.velocity & 0xff;		break;
			case 0x20:	data_ = oscillator.velocity >> 8;		break;
			case 0x40:	data_ = oscillator.volume;				break;
			case 0x60:	data_ = oscillator.sample(ram_);		break;
			case 0x80:	data_ = oscillator.address;				break;
			case 0xa0:	data_ = oscillator.control;				break;
			case 0xc0:	data_ = oscillator.table_size;			break;

			default:
				switch(address_ & 0xff) {
					case 0xe0:
						data_ = DOCInterrupt::no_interrupt();

						// Find the first enabled oscillator that is signalling an interrupt and has interrupts enabled.
						for(int c = 0; c < oscillator_count_; c++) {
							if(oscillators_[c].interrupt_request && (oscillators_[c].control & 0x08)) {
								oscillators_[c].interrupt_request = false;
								data_ = DOCInterrupt::interrupt(uint8_t(c));
								break;
							}
						}
					break;
					case 0xe1:	data_ = uint8_t(oscillator_count_ << 1); 	break;
					case 0xe2:	data_ = 128;								break;	// Input audio.
				}
			break;
		}
	}

	if(control_ & GLUControl::AddressAutoIncrement) {
		++address_;
	}
	return data;
}

// MARK: - Time entry points.

void GLU::run_for(const Cycles duration) {
	const auto cycles = duration.as<uint32_t>();

	// TODO: my current reading is that not every oscillator is updated on every tick; there's a maxmimum
	// quantity per cycle. Check on that.
	for(int c = 0; c < oscillator_count_; c++) {
		// Don't do anything for halted oscillators.
		if(oscillators_[c].control & 1) continue;

		// Update phase.
		oscillators_[c].position += oscillators_[c].velocity * cycles;

		//
		// TODO:
		//
		//	scan rate = [clock rate] / [enabled oscillators + 2]
		//	output frequency = (scan rate  / 2 ^ (17 + channel resolution)) * programmed frequency
		//

		// Check for stops, and any interrupts that therefore flow.
		if((oscillators_[c].control & 2) && (oscillators_[c].position & oscillators_[c].overflow_mask)) {
			// Apply halt, set interrupt request flag.
			oscillators_[c].position = 0;
			oscillators_[c].control |= 1;
			oscillators_[c].interrupt_request = true;
		}
	}
}

bool GLU::interrupt() const {
	// TODO: don't loop through and check every oscillator every time.

	// Return @c true if any oscillator currently has its interrupt request
	// set, and has interrupts enabled.
	for(int c = 0; c < oscillator_count_; c++) {
		if(oscillators_[c].interrupt_request && (oscillators_[c].control & 0x08)) {
			return true;
		}
	}
	return false;
}

// MARK: - Interface boilerplate.

void GLU::set_control(const uint8_t control)	{	control_ = control;			}
uint8_t GLU::control() const					{	return control_;			}
void GLU::set_address_low(const uint8_t low)	{	load_low(address_, low);	}
uint8_t GLU::address_low() const				{	return address_ & 0xff;		}
void GLU::set_address_high(const uint8_t high)	{	load_high(address_, high);	}
uint8_t GLU::address_high() const				{	return address_ >> 8;		}

// MARK: - Update logic.

//Cycles GLU::next_sequence_point() const {
//	uint32_t result = std::numeric_limits<decltype(result)>::max();
//
//	for(int c = 0; c < oscillator_count_; c++) {
//		// Don't do anything for halted oscillators, or for oscillators that can't hit stops.
//		if((oscillators_[c].control&3) != 2) {
//			continue;
//		}
//
//		// Determine how many cycles until a stop is hit and update the pending result
//		// if this is the new soonest-to-expire oscillator.
//		const auto first_overflow_value = (oscillators_[c].overflow_mask - 1) << 1;
//		const auto time_until_stop =
//			(first_overflow_value - oscillators_[c].position + oscillators_[c].velocity - 1)
//				/ oscillators_[c].velocity;
//		result = std::min(result, time_until_stop);
//	}
//	return Cycles(result);
//}

//template <Outputs::Speaker::Action action>
//void GLU::generate_audio(size_t number_of_samples, Outputs::Speaker::MonoSample *target) {
//	auto next_store = pending_stores_[pending_store_read_].load(std::memory_order::acquire);
//	uint8_t next_amplitude = 255;
//	for(size_t sample = 0; sample < number_of_samples; sample++) {
//
//		// TODO: there's a bit of a hack here where it is assumed that the input clock has been
//		// divided in advance. Real hardware divides by 8, I think?
//
//		// Seed output as 0.
//		int output = 0;
//
//		// Apply phase updates to all enabled oscillators.
//		for(int c = 0; c < remote_.oscillator_count; c++) {
//			// Don't do anything for halted oscillators.
//			if(remote_.oscillators[c].control&1) continue;
//
//			remote_.oscillators[c].position += remote_.oscillators[c].velocity;
//
//			// Test for a new halting event.
//			switch(remote_.oscillators[c].control & 6) {
//				case 0:	// Free-run mode; don't truncate the position at all, in case the
//						// accumulator bits in use changes.
//					output += remote_.oscillators[c].output(remote_.ram_);
//				break;
//
//				case 2:	// One-shot mode; check for end of run. Otherwise update sample.
//					if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
//						remote_.oscillators[c].position = 0;
//						remote_.oscillators[c].control |= 1;
//					}
//				break;
//
//				case 4:	// Sync/AM mode.
//					if(c&1) {
//						// Oscillator is odd-numbered; it will amplitude-modulate the next voice.
//						next_amplitude = remote_.oscillators[c].sample(remote_.ram_);
//						continue;
//					} else {
//						// Oscillator is even-numbered; it will 'sync' to the even voice, i.e. any
//						// time it wraps around, it will reset the next oscillator.
//						if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
//							remote_.oscillators[c].position &= remote_.oscillators[c].overflow_mask;
//							remote_.oscillators[c+1].position = 0;
//						}
//					}
//				break;
//
//				case 6:	// Swap mode; possibly trigger partner, and update sample.
//						// Per tech note #11: "Whenever a swap occurs from a higher-numbered
//						// oscillator to a lower-numbered one, the output signal from the corresponding
//						// generator temporarily falls to the zero-crossing level (silence)"
//					if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
//						remote_.oscillators[c].control |= 1;
//						remote_.oscillators[c].position = 0;
//						remote_.oscillators[c^1].control &= ~1;
//					}
//				break;
//			}
//
//			// Don't add output for newly-halted oscillators.
//			if(remote_.oscillators[c].control&1) continue;
//
//			// Append new output.
//			output += (remote_.oscillators[c].output(remote_.ram_) * next_amplitude) / 255;
//			next_amplitude = 255;
//		}
//
//		// Maximum total output was 32 channels times a 16-bit range. Map that down.
//		// TODO: dynamic total volume?
//		Outputs::Speaker::apply<action>(
//			target[sample],
//			Outputs::Speaker::MonoSample(
//				(output * output_range_) >> 20
//			)
//		);
//
//		// Apply any RAM writes that interleave here.
//		++pending_store_read_time_;
//		if(!next_store.enabled) continue;
//		if(next_store.time != pending_store_read_time_) continue;
//		remote_.ram_[next_store.address] = next_store.value;
//		next_store.enabled = false;
//		pending_stores_[pending_store_read_].store(next_store, std::memory_order::relaxed);
//		pending_store_read_ = (pending_store_read_ + 1) & (StoreBufferSize - 1);
//	}
//}

uint8_t GLU::Oscillator::sample(const uint8_t *const ram) {
	// Determines how many you'd have to shift a 16-bit pointer to the right for,
	// in order to hit only the position-supplied bits.
	const int pointer_shift = 8 - ((table_size >> 3) & 7);

	// Table size mask should be 0x8000 for the largest table size, and 0xff00 for
	// the smallest.
	const uint16_t table_size_mask = 0xffff >> pointer_shift;

	// The pointer should use (at most) 15 bits; starting with bit 1 for resolution 0
	// and starting at bit 8 for resolution 7.
	const uint16_t table_pointer = uint16_t(position >> ((table_size&7) + pointer_shift));

	// The full pointer is composed of the bits of the programmed address not touched by
	// the table pointer, plus the table pointer.
	const auto sample_address = uint16_t(((address << 8) & ~table_size_mask) | (table_pointer & table_size_mask));

	// Ignored here: bit 6 should select between RAM banks. But for now this is IIgs-centric,
	// and that has only one bank of RAM.
	return ram[sample_address];
}

int16_t GLU::Oscillator::output(const uint8_t *const ram) {
	const auto level = sample(ram);

	// "An oscillator will halt when a zero is encountered in its waveform table."
	// TODO: only if in free-run mode, I think? Or?
	if(!level) {
		control |= 1;
		return 0;
	}

	// Samples are unsigned 8-bit; do the proper work to make volume work correctly.
	return int8_t(level ^ 128) * volume;
}
