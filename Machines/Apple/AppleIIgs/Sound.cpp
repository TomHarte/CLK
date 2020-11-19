//
//  Sound.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Sound.hpp"

#include <cstdio>

// TODO: is it safe not to check for back-pressure in pending_stores_?

using namespace Apple::IIgs::Sound;

GLU::GLU(Concurrency::DeferringAsyncTaskQueue &audio_queue) : audio_queue_(audio_queue) {}

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
		audio_queue_.defer([this, address, data] () {
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
		case 0xa0:
			oscillators[address & 0x1f].control = value;
		break;
		case 0xc0:
			oscillators[address & 0x1f].table_size = value;
		break;

		default:
			switch(address & 0xff) {
				case 0xe0:
					/* Does setting the interrupt register really make any sense? */
					interrupt_state = value;
				break;
				case 0xe1:
					oscillator_count = (value >> 1) ? (value >> 1) : 1;
				break;
				case 0xe2:
					/* Writing to the analogue to digital input definitely makes no sense. */
				break;
			}
		break;
	}
}

uint8_t GLU::get_data() {
	// TODO: all of this. From local_, with just-in-time generation of the data sample and AD values.
	return 0;
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
	generate_audio(number_of_samples, target, output_range_);
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
	audio_queue_.defer([this, control] () {
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

void GLU::generate_audio(size_t number_of_samples, std::int16_t *target, int16_t range) {
	(void)number_of_samples;
	(void)target;
	(void)range;

	auto next_store = pending_stores_[pending_store_read_].load(std::memory_order::memory_order_acquire);
	for(size_t c = 0; c < number_of_samples; c++) {
		target[c] = 0;

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

void GLU::skip_audio(EnsoniqState &state, size_t number_of_samples) {
	(void)number_of_samples;
	(void)state;

	// Just advance all oscillator pointers and check for interrupts.
	// If a read occurs to the current-output level, generate it then.
}
