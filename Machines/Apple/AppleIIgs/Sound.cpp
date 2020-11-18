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
		printf("Register write %04x\n", address_);
	}

	if(control_ & 0x20) {
		++address_;
	}
}

uint8_t GLU::get_data() {
	return 0;
}

// MARK: - Update logic.

void GLU::run_for(Cycles cycles) {
	// Update local state, without generating audio.
}

void GLU::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// Update remote state, generating audio.
}

void GLU::skip_samples(const std::size_t number_of_samples) {
	// Update remote state, without generating audio.
}

void GLU::set_sample_volume_range(std::int16_t range) {
	// TODO
}

// MARK: - Interface boilerplate.

void GLU::set_control(uint8_t control) {
	control_ = control;

	// Low three bits are volume control: this probably needs to be fed off-thrad?
}

uint8_t GLU::get_control() {
	return control_;
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
