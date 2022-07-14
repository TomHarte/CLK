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

// TODO: is it safe not to check for back-pressure in pending_stores_?

using namespace Apple::IIgs::Sound;

GLU::GLU(Concurrency::TaskQueue<false> &audio_queue) : audio_queue_(audio_queue) {
	// Reset all pending stores.
	MemoryWrite disabled_write;
	disabled_write.enabled = false;
	for(int c = 0; c < StoreBufferSize; c++) {
		pending_stores_[c].store(disabled_write);
	}
}

void GLU::set_data(uint8_t data) {
	if(local_.control & 0x40) {
		// RAM access.
		local_.ram_[address_] = data;

		MemoryWrite write;
		write.enabled = true;
		write.address = address_;
		write.value = data;
		write.time = pending_store_write_time_;
		pending_stores_[pending_store_write_].store(write, std::memory_order::memory_order_release);

		pending_store_write_ = (pending_store_write_ + 1) % (StoreBufferSize - 1);
	} else {
		// Register access.
		const auto address = address_;	// To make sure I don't inadvertently 'capture' address_.
		local_.set_register(address, data);
		audio_queue_.enqueue([this, address, data] () {
			remote_.set_register(address, data);
		});
	}

	if(local_.control & 0x20) {
		++address_;
	}
}

void GLU::EnsoniqState::set_register(uint16_t address, uint8_t value) {
	switch(address & 0xe0) {
		case 0x00:
			oscillators[address & 0x1f].velocity = uint16_t((oscillators[address & 0x1f].velocity & 0xff00) | (value << 0));
		break;
		case 0x20:
			oscillators[address & 0x1f].velocity = uint16_t((oscillators[address & 0x1f].velocity & 0x00ff) | (value << 8));
		break;
		case 0x40:
			oscillators[address & 0x1f].volume = value;
		break;
		case 0x60:
			/* Does setting the last sample make any sense? */
		break;
		case 0x80:
			oscillators[address & 0x1f].address = value;
		break;
		case 0xa0: {
			oscillators[address & 0x1f].control = value;

			// Halt + M0 => reset position.
			if((oscillators[address & 0x1f].control & 0x3) == 3) {
				oscillators[address & 0x1f].control |= 1;
			}
		} break;
		case 0xc0:
			oscillators[address & 0x1f].table_size = value;

			// The most-significant bit that should be used is 16 + (value & 7).
			oscillators[address & 0x1f].overflow_mask = ~(0xffffff >> (7 - (value & 7)));
		break;

		default:
			switch(address & 0xff) {
				case 0xe0:
					/* Does setting the interrupt register really make any sense? */
				break;
				case 0xe1:
					oscillator_count = 1 + ((value >> 1) & 31);
				break;
				case 0xe2:
					/* Writing to the analogue to digital input definitely makes no sense. */
				break;
			}
		break;
	}
}

uint8_t GLU::get_data() {
	const auto address = address_;
	if(local_.control & 0x20) {
		++address_;
	}

	switch(address & 0xe0) {
		case 0x00:	return local_.oscillators[address & 0x1f].velocity & 0xff;
		case 0x20:	return local_.oscillators[address & 0x1f].velocity >> 8;;
		case 0x40:	return local_.oscillators[address & 0x1f].volume;
		case 0x60:	return local_.oscillators[address & 0x1f].sample(local_.ram_);	// i.e. look up what the sample was on demand.
		case 0x80:	return local_.oscillators[address & 0x1f].address;
		case 0xa0:	return local_.oscillators[address & 0x1f].control;
		case 0xc0:	return local_.oscillators[address & 0x1f].table_size;

		default:
			switch(address & 0xff) {
				case 0xe0: {
					// Find the first enabled oscillator that is signalling an interrupt and has interrupts enabled.
					for(int c = 0; c < local_.oscillator_count; c++) {
						if(local_.oscillators[c].interrupt_request && (local_.oscillators[c].control & 0x08)) {
							local_.oscillators[c].interrupt_request = false;
							return uint8_t(0x41 | (c << 1));
						}
					}

					// No interrupt found.
					return 0xc1;
				} break;
				case 0xe1:	return uint8_t((local_.oscillator_count - 1) << 1);	// TODO: should other bits be 0 or 1?
				case 0xe2:	return 128;											// Input audio. Unimplemented!
			}
		break;
	}

	return 0;
}

bool GLU::get_interrupt_line() {
	// Return @c true if any oscillator currently has its interrupt request
	// set, and has interrupts enabled.
	for(int c = 0; c < local_.oscillator_count; c++) {
		if(local_.oscillators[c].interrupt_request && (local_.oscillators[c].control & 0x08)) {
			return true;
		}
	}
	return false;
}

// MARK: - Time entry points.

void GLU::run_for(Cycles cycles) {
	// Update local state, without generating audio.
	skip_audio(local_, cycles.as<size_t>());

	// Update the timestamp for memory writes;
	pending_store_write_time_ += cycles.as<uint32_t>();
}

void GLU::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// Update remote state, generating audio.
	generate_audio(number_of_samples, target);
}

void GLU::skip_samples(const std::size_t number_of_samples) {
	// Update remote state, without generating audio.
	skip_audio(remote_, number_of_samples);

	// Apply any pending stores.
	std::atomic_thread_fence(std::memory_order::memory_order_acquire);
	const uint32_t final_time = pending_store_read_time_ + uint32_t(number_of_samples);
	while(true) {
		auto next_store = pending_stores_[pending_store_read_].load(std::memory_order::memory_order_acquire);
		if(!next_store.enabled) break;
		if(next_store.time >= final_time) break;
		remote_.ram_[next_store.address] = next_store.value;
		next_store.enabled = false;
		pending_stores_[pending_store_read_].store(next_store, std::memory_order::memory_order_relaxed);

		pending_store_read_ = (pending_store_read_ + 1) & (StoreBufferSize - 1);
	}
}

void GLU::set_sample_volume_range(std::int16_t range) {
	output_range_ = range;
}

// MARK: - Interface boilerplate.

void GLU::set_control(uint8_t control) {
	local_.control = control;
	audio_queue_.enqueue([this, control] () {
		remote_.control = control;
	});
}

uint8_t GLU::get_control() {
	return local_.control;
}

void GLU::set_address_low(uint8_t low) {
	address_ = uint16_t((address_ & 0xff00) | low);
}

uint8_t GLU::get_address_low() {
	return address_ & 0xff;
}

void GLU::set_address_high(uint8_t high) {
	address_ = uint16_t((high << 8) | (address_ & 0x00ff));
}

uint8_t GLU::get_address_high() {
	return address_ >> 8;
}

// MARK: - Update logic.

Cycles GLU::get_next_sequence_point() const {
	uint32_t result = std::numeric_limits<decltype(result)>::max();

	for(int c = 0; c < local_.oscillator_count; c++) {
		// Don't do anything for halted oscillators, or for oscillators that can't hit stops.
		if((local_.oscillators[c].control&3) != 2) {
			continue;
		}

		// Determine how many cycles until a stop is hit and update the pending result
		// if this is the new soonest-to-expire oscillator.
		const auto first_overflow_value = (local_.oscillators[c].overflow_mask - 1) << 1;
		const auto time_until_stop = (first_overflow_value - local_.oscillators[c].position + local_.oscillators[c].velocity - 1) / local_.oscillators[c].velocity;
		result = std::min(result, time_until_stop);
	}
	return Cycles(result);
}

void GLU::skip_audio(EnsoniqState &state, size_t number_of_samples) {
	// Just advance all oscillator pointers and check for interrupts.
	// If a read occurs to the current-output level, generate it then.
	for(int c = 0; c < state.oscillator_count; c++) {
		// Don't do anything for halted oscillators.
		if(state.oscillators[c].control&1) continue;

		// Update phase.
		state.oscillators[c].position += state.oscillators[c].velocity * number_of_samples;

		// Check for stops, and any interrupts that therefore flow.
		if((state.oscillators[c].control & 2) && (state.oscillators[c].position & state.oscillators[c].overflow_mask)) {
			// Apply halt, set interrupt request flag.
			state.oscillators[c].position = 0;
			state.oscillators[c].control |= 1;
			state.oscillators[c].interrupt_request = true;
		}
	}
}

void GLU::generate_audio(size_t number_of_samples, std::int16_t *target) {
	auto next_store = pending_stores_[pending_store_read_].load(std::memory_order::memory_order_acquire);
	uint8_t next_amplitude = 255;
	for(size_t sample = 0; sample < number_of_samples; sample++) {

		// TODO: there's a bit of a hack here where it is assumed that the input clock has been
		// divided in advance. Real hardware divides by 8, I think?

		// Seed output as 0.
		int output = 0;

		// Apply phase updates to all enabled oscillators.
		for(int c = 0; c < remote_.oscillator_count; c++) {
			// Don't do anything for halted oscillators.
			if(remote_.oscillators[c].control&1) continue;

			remote_.oscillators[c].position += remote_.oscillators[c].velocity;

			// Test for a new halting event.
			switch(remote_.oscillators[c].control & 6) {
				case 0:	// Free-run mode; don't truncate the position at all, in case the
						// accumulator bits in use changes.
					output += remote_.oscillators[c].output(remote_.ram_);
				break;

				case 2:	// One-shot mode; check for end of run. Otherwise update sample.
					if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
						remote_.oscillators[c].position = 0;
						remote_.oscillators[c].control |= 1;
					}
				break;

				case 4:	// Sync/AM mode.
					if(c&1) {
						// Oscillator is odd-numbered; it will amplitude-modulate the next voice.
						next_amplitude = remote_.oscillators[c].sample(remote_.ram_);
						continue;
					} else {
						// Oscillator is even-numbered; it will 'sync' to the even voice, i.e. any
						// time it wraps around, it will reset the next oscillator.
						if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
							remote_.oscillators[c].position &= remote_.oscillators[c].overflow_mask;
							remote_.oscillators[c+1].position = 0;
						}
					}
				break;

				case 6:	// Swap mode; possibly trigger partner, and update sample.
						// Per tech note #11: "Whenever a swap occurs from a higher-numbered
						// oscillator to a lower-numbered one, the output signal from the corresponding
						// generator temporarily falls to the zero-crossing level (silence)"
					if(remote_.oscillators[c].position & remote_.oscillators[c].overflow_mask) {
						remote_.oscillators[c].control |= 1;
						remote_.oscillators[c].position = 0;
						remote_.oscillators[c^1].control &= ~1;
					}
				break;
			}

			// Don't add output for newly-halted oscillators.
			if(remote_.oscillators[c].control&1) continue;

			// Append new output.
			output += (remote_.oscillators[c].output(remote_.ram_) * next_amplitude) / 255;
			next_amplitude = 255;
		}

		// Maximum total output was 32 channels times a 16-bit range. Map that down.
		// TODO: dynamic total volume?
		target[sample] = (output * output_range_) >> 20;

		// Apply any RAM writes that interleave here.
		++pending_store_read_time_;
		if(!next_store.enabled) continue;
		if(next_store.time != pending_store_read_time_) continue;
		remote_.ram_[next_store.address] = next_store.value;
		next_store.enabled = false;
		pending_stores_[pending_store_read_].store(next_store, std::memory_order::memory_order_relaxed);
		pending_store_read_ = (pending_store_read_ + 1) & (StoreBufferSize - 1);
	}
}

uint8_t GLU::EnsoniqState::Oscillator::sample(uint8_t *ram) {
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
	const uint16_t sample_address = ((address << 8) & ~table_size_mask) | (table_pointer & table_size_mask);

	// Ignored here: bit 6 should select between RAM banks. But for now this is IIgs-centric,
	// and that has only one bank of RAM.
	return ram[sample_address];
}

int16_t GLU::EnsoniqState::Oscillator::output(uint8_t *ram) {
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
