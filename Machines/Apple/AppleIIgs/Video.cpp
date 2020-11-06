//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Apple::IIgs::Video;

namespace {

constexpr int CyclesPerLine = 910;
constexpr int Lines = 263;
constexpr int FinalPixelLine = 192;

}

VideoBase::VideoBase() :
	VideoSwitches<Cycles>(Cycles(2), [] (Cycles) {}) {
}

void VideoBase::set_internal_ram(const uint8_t *ram) {
	ram_ = ram;
}

void VideoBase::did_set_annunciator_3(bool) {}
void VideoBase::did_set_alternative_character_set(bool) {}

void VideoBase::run_for(Cycles cycles) {
	// TODO: everything else!
	const auto old = cycles_into_frame_;
	cycles_into_frame_ = (cycles_into_frame_ + cycles.as<int>()) % (CyclesPerLine * Lines);

	// DEBUGGING HACK!!
	// Scan the output buffer, assuming this is 40-column text mode, and print anything found.
	if(cycles_into_frame_ < old) {
		for(int line = 0; line < 192; line += 8) {
			const uint16_t address = get_row_address(line);

			bool did_print_line = false;
			for(int column = 0; column < 40; column++) {
				const char c = char(ram_[address + column]);
				if(c > 0) {
					printf("%c", c);
					did_print_line = true;
				}
			}
			if(did_print_line) printf("\n");
		}
	}
}

bool VideoBase::get_is_vertical_blank() {
	return cycles_into_frame_ >= FinalPixelLine * CyclesPerLine;
}

void VideoBase::set_new_video(uint8_t new_video) {
	new_video_ = new_video;
}

uint8_t VideoBase::get_new_video() {
	return new_video_;
}

void VideoBase::clear_interrupts(uint8_t mask) {
	set_interrupts(interrupts_ & ~(mask & 0x60));
}

void VideoBase::set_interrupt_register(uint8_t mask) {
	set_interrupts(interrupts_ | (mask & 0x6));
}

uint8_t VideoBase::get_interrupt_register() {
	return interrupts_;
}

void VideoBase::notify_clock_tick() {
	set_interrupts(interrupts_ | 0x40);
}

void VideoBase::set_interrupts(uint8_t new_value) {
	interrupts_ = new_value & 0x7f;
	if((interrupts_ >> 4) & interrupts_ & 0x6)
		interrupts_ |= 0x80;
}
