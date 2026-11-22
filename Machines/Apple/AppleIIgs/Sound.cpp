//
//  Sound.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Sound.hpp"

#include "Machines/Utility/MemoryFuzzer.hpp"

#include <algorithm>
#include <bit>
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
		// Resolution 0 => A16 is the highest bit selected; resolution 1 => A17; etc.
		return uint32_t((1 << (17 + resolution(size))) - 1);
	}

	static constexpr bool overflow(const uint8_t size, const uint32_t accumulator) {
		return accumulator > accumulator_mask(size);
	}

	static constexpr uint16_t address(const uint8_t size, const uint32_t accumulator, const uint8_t pointer) {
		// At least 8 bits of the output address always come from the accumulator, and possibly as many as 15.
		// The table size is the number of bits greater than 8.
		//
		// i.e.
		//	table_size = 0 => 8 bits of address from the accumulator;
		//	table_size = 1 => 9 bits of address from the accumulator;
		//	etc.
		const auto table_size = (size & 0b111'000) >> 3;

		// High bits of the pointer are always used; lower bits are used only if they haven't
		// been replaced by the accumulator.
		const auto pointer_mask = 0xff00 << table_size;

		// The accumulator is naturally 24 bits, so a right shift of 16 would put its highest order bits
		// into the low 8 bits of the address.
		//
		// From there, it's one less shift for each fewer bit of the accumulator being used — one less for each
		// "extra" accumulator bit.
		//
		// A 'resolution' of 7 implies to make no further adjustments from there. A 'resolution' of 0 would
		// mean to discard the high 7 bits of the accumulator. So it effects a left shift.
		const auto accumulator_shift = 16 - table_size - (7 - resolution(size));

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

GLU::GLU(float clock_rate) noexcept : speaker_(clock_rate, 10'000) {
	// Per "Cortland Sound ERS" a 10 kHz lowpass filter applies.

	Memory::Fuzz(ram_);
}

void GLU::set_data(const uint8_t data) {
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
			clear_interrupt(oscillator_id);

			// Halt + M0 => reset position.
			if((oscillator.control & 0x3) == 3) {
				oscillator.position = 0;
			}
		} break;

		default:
			switch(address & 0xff) {
				case 0xe0:
					/* Does setting the interrupt register really make any sense? */
				break;
				case 0xe1:
					oscillator_count_ = 1 + ((value >> 1) & 31);
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
	// time around if accessing RAM.
	const auto result = data_;
	data_ = [&]() -> uint8_t {
		if(control_ & GLUControl::AccessRAM) {
			return ram_[address_];
		} else {
			auto &oscillator = oscillators_[address_ & 0b0001'1111];
			switch(address_ & 0b1110'0000) {
				case 0x00:	return oscillator.velocity & 0xff;
				case 0x20:	return oscillator.velocity >> 8;
				case 0x40:	return oscillator.volume;
				case 0x60:	return oscillator.sample;
				case 0x80:	return oscillator.address;
				case 0xa0:	return oscillator.control;
				case 0xc0:	return oscillator.table_size;

				default:
					switch(address_ & 0xff) {
						case 0xe0:
							// Find the first enabled oscillator signalling an interrupt.
							if(interrupts_) {
								const auto id = std::countr_zero(interrupts_);
								clear_interrupt(id);
								return DOCInterrupt::interrupt(uint8_t(id));
							} else {
								return DOCInterrupt::no_interrupt();
							}

						case 0xe1:	return 0b11'00000'1 | uint8_t((oscillator_count_ - 1) << 1);
						case 0xe2:	return 128;
							// Input audio is fixed at '128'.

						default: return 0xff;
					}
				break;
			}
		}
	} ();

	if(control_ & GLUControl::AddressAutoIncrement) {
		++address_;
	}
	return result;
}

// MARK: - Time entry points.

void GLU::toggle() {
	toggle_state_ ^= true;
	toggle_ = toggle_state_ ? ToggleRange : -ToggleRange;
}

void GLU::flush() {
	speaker_.flush();
}

void GLU::run_for(const Cycles duration) {
	const auto cycles = duration.as<int>();

	for(int c = 0; c < cycles; c++) {
		// Set the proportion of total available volume that is used by the DOC (as opposed to the toggle).
		static constexpr int DocVolumeNumerator = 8;
		static constexpr int DocVolumeDenominator = 9;
		static_assert(DocVolumeDenominator > DocVolumeNumerator);

		// ... implies the toggle numerator.
		static constexpr int ToggleNumerator = DocVolumeDenominator - DocVolumeNumerator;

		// Total divide will be the denominator * 15 to account for the GLU's volume control.
		static constexpr int Denominator = DocVolumeDenominator * 15;

		const int glu_volume = control_ & GLUControl::Volume;
		const int numerator = ((toggle_ * ToggleNumerator) + (output_ * DocVolumeNumerator)) * glu_volume;
		const auto mix = int16_t(numerator / Denominator);

		speaker_.push_back(mix);

		//
		// Apple's documentation provides a frequency formula of:
		//
		//	scan rate = [clock rate] / [enabled oscillators + 2]
		//	output frequency = (scan rate  / 2 ^ (17 + channel resolution)) * programmed frequency
		//
		// My guess is therefore that at most one oscillator is updated per clock cycle.
		//
		++next_oscillator_;
		if(next_oscillator_ >= oscillator_count_) {
			next_oscillator_ = -2;
		}
		if(next_oscillator_ < 0) {
			continue;
		}

		auto &oscillator = oscillators_[size_t(next_oscillator_)];

		// Sample before updating phase to ensure that straight after reset the first thing fetched really
		// is the start of the sample.
		const uint8_t sample = oscillator.latch_sample(ram_);
		output_ = oscillator.scaled_output(next_oscillator_, sample);

		// Don't do anything for halted oscillators.
		if(oscillator.control & OscillatorControl::Halted) continue;

		// Update phase.
		oscillator.position += oscillator.velocity;

		// "The oscillator will halt when the halt bit is set or when a 0 is encountered in the table data."
		// Apple documented this as being free-run behaviour only, but online commentary suggests it affects
		// all oscillator modes. It also counts as end of sample.
		oscillator.control |= sample ? 0 : OscillatorControl::Halted;

		const bool overflow = WavetableSize::overflow(oscillator.table_size, oscillator.position);
		const bool ended = overflow || !sample;
		oscillator.position &= WavetableSize::accumulator_mask(oscillator.table_size);

		// "When an oscillator reaches the end of a wavetable and the enable interrupt (EI) bit for that oscillator has
		// previously been set, the IRQ line and bit 7 of the Oscillator Interrupt register are then set"
		if(ended && (oscillator.control & OscillatorControl::InterruptEnabled)) {
			set_interrupt(next_oscillator_);
		}

		const auto pair = [&]() -> Oscillator & {
			return oscillators_[size_t(next_oscillator_ ^ 1)];
		};

		switch(OscillatorControl::mode(oscillator.control)) {
			using enum OscillatorControl::Mode;

			case FreeRun:
				// "The oscillator begins at the beginning of the wavetable and repeats the same wavetable."
			break;

			case OneShot:
				// "The oscillator begins at the beginning of the wavetable, stepping through it only once,
				// and stopping at the end of the table."
				if(overflow) {
					oscillator.control |= OscillatorControl::Halted;
					oscillator.position = 0;
				}
			break;

			case Sync:
				// "Enable sync mode by selecting even/odd pairs of oscillators (a lower even-numbered oscillator
				// paired with an adjacent higher-numbered oscillator). When the even-numbered oscillator begins its
				// wavetable, the odd-mate oscillator will synchronise and begin its wavetable simultaneously."
				//
				// Ensoniq documentation reports this as being amplitude modulation in the other direction, i.e. if
				// enabled on an odd-numbered oscillator. So implement that also.

				if(next_oscillator_ & 1) {
					const auto am_oscillator = size_t(next_oscillator_ + 1);
					if(am_oscillator < oscillators_.size()) {
						oscillators_[am_oscillator].volume = sample;
					}
				} else {
					if(overflow) {
						oscillator.position = pair().position = 0;
					}
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
				}
			break;
		}
	}
}

bool GLU::interrupt() const {
	return bool(interrupts_);
}

void GLU::set_interrupt(const int id) {
	interrupts_ |= 1u << id;
}

void GLU::clear_interrupt(const int id) {
	interrupts_ &= ~(1u << id);
}

// MARK: - Interface boilerplate.

void GLU::set_control(const uint8_t control)	{	control_ = control & ~GLUControl::DOCBusy;	}
uint8_t GLU::control() const					{	return control_;							}
void GLU::set_address_low(const uint8_t low)	{	load_low(address_, low);					}
uint8_t GLU::address_low() const				{	return address_ & 0xff;						}
void GLU::set_address_high(const uint8_t high)	{	load_high(address_, high);					}
uint8_t GLU::address_high() const				{	return address_ >> 8;						}

// MARK: - Update logic.

uint8_t GLU::Oscillator::latch_sample(const uint8_t *const ram) {
	const uint8_t value = ram[WavetableSize::address(table_size, position, address)];
	if(value) sample = value;
	return value;
}

int16_t GLU::Oscillator::scaled_output(const int id, const uint8_t contribution) const {
	// Halted channels do not contribute to the output.
	if(control & OscillatorControl::Halted) return 0;

	// AM sources also do not contribute.
	if((id & 1) && OscillatorControl::mode(control) == OscillatorControl::Mode::Sync) return 0;

	// Samples are unsigned 8-bit; scale to signed 16-bit, minus some headroom for the toggle speaker.
	return int8_t(contribution ^ 0x80) * volume;
}
