//
//  Sound.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Sound.hpp"

#include <cstdio>

using namespace Apple::IIgs::Sound;

GLU::GLU(Concurrency::DeferringAsyncTaskQueue &audio_queue) : audio_queue_(audio_queue) {}

void GLU::set_data(uint8_t data) {
	if(control_ & 0x40) {
		// RAM access.
		local_.ram_[address_] = data;
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

	if(control_ & 0x20) {
		++address_;
	}
}

uint8_t GLU::get_data() {
	return 0;
}

// MARK: - Time entry points.

void GLU::run_for(Cycles cycles) {
	// Update local state, without generating audio.
	local_.skip_audio(cycles.as<size_t>());
}

void GLU::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// Update remote state, generating audio.
	remote_.generate_audio(number_of_samples, target, output_range_);
}

void GLU::skip_samples(const std::size_t number_of_samples) {
	// Update remote state, without generating audio.
	remote_.skip_audio(number_of_samples);
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

void GLU::EnsoniqState::generate_audio(size_t number_of_samples, std::int16_t *target, int16_t range) {
	(void)number_of_samples;
	(void)target;
	(void)range;

	memset(target, 0, number_of_samples * sizeof(int16_t));
}

void GLU::EnsoniqState::skip_audio(size_t number_of_samples) {
	(void)number_of_samples;

	// Just advance all oscillator pointers and check for interrupts.
	// If a read occurs to the current-output level, generate it then.
}
