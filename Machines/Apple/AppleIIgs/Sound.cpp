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
	static constexpr uint8_t Volume								= 0b0000'1111;
	static constexpr uint8_t Reserved [[maybe_unused]] 			= 0b0001'0000;
	static constexpr uint8_t AddressAutoIncrement				= 0b0010'0000;
	static constexpr uint8_t AccessRAM							= 0b0100'0000;
	static constexpr uint8_t DOCBusy [[maybe_unused]]			= 0b1000'0000;
};

struct DOCInterrupt {
	static constexpr uint8_t NoInterrupt						= 0b1000'0000;
	static constexpr uint8_t Reserved [[maybe_unused]]			= 0b0100'0001;
	static constexpr uint8_t Oscillator	[[maybe_unused]]		= 0b0011'1110;

	static constexpr uint8_t interrupt(const uint8_t oscillator) {
		return uint8_t((oscillator << 1) | Reserved);
	}
	static constexpr uint8_t no_interrupt() {
		return NoInterrupt | Reserved;
	}
};

struct OscillatorControl {
	static constexpr uint8_t Halted								= 0b0000'0001;
	static constexpr uint8_t InterruptEnabled					= 0b0000'1000;
	static constexpr uint8_t ChannelAddress [[maybe_unused]]	= 0b1111'0000;

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
	static constexpr uint32_t accumulator_mask(const uint8_t size) {
		return uint32_t(~(0xff'ffff >> resolution(size)));
	}

	static constexpr bool overflow(const uint8_t size, const uint32_t accumulator) {
		return accumulator & accumulator_mask(size);
	}

	static constexpr uint16_t address(const uint8_t size, const uint32_t accumulator, const uint8_t pointer) {
		// At least 8 bits of the output address always come from the accumulator, and possibly as many as 15.
		// The 'extra' bits is the number of bits greater than 8.
		//
		// i.e.
		//	extra_accumulator_bits = 0 => 8 bits of address from the accumulator;
		//	extra_accumulator_bits = 1 => 9 bits of address from the accumulator;
		//	etc.
		const auto extra_accumulator_bits = (size & 0b111'000) >> 3;

		// High bits of the pointer are always used; lower bits are used only if they haven't
		// been replaced by the accumulator.
		const auto pointer_mask = 0xff00 << extra_accumulator_bits;

		// The accumulator is naturally 24 bits, so a right shift of 16 would put its highest order bits
		// into the low 8 bits of the address.
		//
		// From there, it's one less shift for each fewer bit fo the accumulator being used — one less for each
		// "extra" accumulator bit.
		//
		// A 'resolution' of 7 implies to make no further adjustments from there. A 'resolution' of 0 would
		// mean to discard the high 7 bits of the accumulator. So it effects a left shift.
		const auto accumulator_shift = 16 - extra_accumulator_bits - (7 - resolution(size));

		return
			(uint16_t(pointer << 8) & pointer_mask) |
			(uint16_t(accumulator >> accumulator_shift) & ~pointer_mask);
	}

private:

	static constexpr uint8_t resolution(const uint8_t size) {
		return size & 0b000'111;
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

GLU::GLU(float clock_rate) noexcept : speaker_(clock_rate) {}

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
	const auto oscillator_id = address_ & 0b0001'1111;
	auto &oscillator = oscillators_[size_t(oscillator_id)];
	switch(address_ & 0b1110'0000) {
		case 0x00:	load_low(oscillator.velocity, value);		break;
		case 0x20:	load_high(oscillator.velocity, value);		break;
		case 0x40:	oscillator.volume = value;					break;
		case 0x80:	oscillator.address = value;					break;
		case 0xc0:	oscillator.table_size = value;				break;

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

		default:
			switch(address & 0xff) {
				case 0xe0:
					/* Does setting the interrupt register really make any sense? */
				break;
				case 0xe1:
					oscillator_count_ = 1 | ((value >> 1) & 31);

					output_ = 0;
					for(int c = 0; c < oscillator_count_; c++) {
						output_ += oscillators_[size_t(c)].posted_output_;
					}
				break;
				case 0xe2:
					/* Writing to the analogue to digital input definitely makes no sense. */
				break;
			}
		break;
	}

	if(oscillator_id < oscillator_count_) {
		update_output(oscillator);
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
							if(oscillator.interrupt_active()) {
								oscillators_[size_t(c)].interrupt = false;
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

void GLU::toggle() {
	toggle_ ^= 0xffff;
}

void GLU::run_for(const Cycles duration) {
	const auto cycles = duration.as<int>();

	for(int c = 0; c < cycles; c++) {
		speaker_.push_back(int16_t(
			(toggle_ + (output_ * (control_ & GLUControl::Volume)) / 15)
				/ int32_t(oscillators_.size() + 1))
		);

		//
		// Apple's documentation provides a frequency formula of:
		//
		//	scan rate = [clock rate] / [enabled oscillators + 2]
		//	output frequency = (scan rate  / 2 ^ (17 + channel resolution)) * programmed frequency
		//
		// My guess is therefore that at most one oscillator is updated per clock cycle.
		//

		if(next_oscillator_ >= oscillator_count_) {
			next_oscillator_ = -2;
		}
		++next_oscillator_;
		if(next_oscillator_ < 0) {
			continue;
		}

		auto &oscillator = oscillators_[size_t(next_oscillator_)];

		// Don't do anything for halted oscillators.
		if(oscillator.control & OscillatorControl::Halted) continue;

		// Update phase.
		oscillator.position += oscillator.velocity;
		const bool overflow = WavetableSize::overflow(oscillator.table_size, oscillator.position);
		oscillator.position &= WavetableSize::accumulator_mask(oscillator.table_size);

		// "When an oscillator reaches the end of a wavetable and the enable interrupt (EI) bit for that oscillator has
		// previously been set, the IRQ line and bit 7 of the Oscillator Interrupt register are then set"
		oscillator.interrupt |= overflow && (oscillator.control & OscillatorControl::InterruptEnabled);

		const auto pair = [&]() -> Oscillator & {
			return oscillators_[size_t(next_oscillator_ ^ 1)];
		};

		switch(OscillatorControl::mode(oscillator.control)) {
			using enum OscillatorControl::Mode;

			case FreeRun:
				// "The oscillator begins at the beginning of the wavetable and repeats the same wavetable. The
				// oscillator will halt when the halt bit is set or when a 0 is encountered in the table data."
				oscillator.control |= oscillator.sample(ram_) ? 0 : OscillatorControl::Halted;
			break;

			case OneShot:
				// "The oscillator begins at the beginning of the wavetable, stepping through it only once,
				// and stopping at the end of the table."
				oscillator.control |= overflow ? OscillatorControl::Halted : 0;
			break;

			case Sync:
				// "Enable sync mode by selecting even/odd pairs of oscillators (a lower even-numbered oscillator
				// paired with an adjacent higher-numbered oscillator). When the even-numbered oscillator begins its
				// wavetable, the odd-mate oscillator will synchronise and begin its wavetable simultaneously."

				if(overflow) {
					pair().position = 0;
					update_output(pair());
				}
			break;

			case Swap:
				// "Uses even/odd pairs of oscillators (a lower even-numbered oscillator paired with an adjacent
				// higher odd-numbered oscillator). The enabled oscillator pair runs in one-shot mode. When it reaches
				// the end of its wavetable, it resets its accumulator to 0, sets its halt bit, and clears the halt
				// bit of its mate."
				if(overflow) {
					oscillator.position = 0;
					oscillator.control |= OscillatorControl::Halted;
					pair().control &= ~OscillatorControl::Halted;
					update_output(pair());
				}
			break;
		}

		update_output(oscillator);
	}
}

void GLU::update_output(Oscillator &oscillator) {
	output_ -= oscillator.posted_output_;
	oscillator.posted_output_ = oscillator.output(ram_);
	output_ += oscillator.posted_output_;
}

bool GLU::interrupt() const {
	// TODO: don't loop through and check every oscillator every time.

	// Return @c true if any oscillator currently has its interrupt request
	// set, and has interrupts enabled.
	for(int c = 0; c < oscillator_count_; c++) {
		if(oscillators_[size_t(c)].interrupt_active()) {
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

bool GLU::Oscillator::interrupt_active() const {
	return interrupt && (control & OscillatorControl::InterruptEnabled);
}

uint8_t GLU::Oscillator::sample(const uint8_t *const ram) const {
	return ram[WavetableSize::address(table_size, position, address)];
}

int16_t GLU::Oscillator::output(const uint8_t *const ram) const {
	const auto level = sample(ram);

	// Samples are unsigned 8-bit; scale to signed 16-bit.
	return (level * volume) ^ 0x8000;
}
