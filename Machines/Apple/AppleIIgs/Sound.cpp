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
		switch(address_ & 0xe0) {
			case 0x00:
//				oscillators_[address_ & 0x1f].velocity = (oscillators_[address_ & 0x1f].velocity & 0xff00) | (data << 8);
			break;
			case 0x20:
//				oscillators_[address_ & 0x1f].velocity = (oscillators_[address_ & 0x1f].velocity & 0x00ff) | (data << 8);
			break;
		}
//		printf("Register write %04x\n", address_);
	}

	if(local_.control & 0x20) {
		++address_;
	}
}

uint8_t GLU::get_data() {
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
