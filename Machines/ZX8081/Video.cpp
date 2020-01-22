//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include <algorithm>

using namespace ZX8081;

namespace {

/*!
	The number of bytes of PCM data to allocate at once; if/when more are required,
	the class will simply allocate another batch.
*/
const std::size_t StandardAllocationSize = 320;

}

Video::Video() :
	crt_(207 * 2, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Luminance1) {

	// Show only the centre 80% of the TV frame.
	crt_.set_display_type(Outputs::Display::DisplayType::CompositeMonochrome);
	crt_.set_visible_area(Outputs::Display::Rect(0.1f, 0.1f, 0.8f, 0.8f));
}

void Video::run_for(const HalfCycles half_cycles) {
	// Just keep a running total of the amount of time that remains owed to the CRT.
	time_since_update_ += half_cycles;
}

void Video::flush() {
	flush(sync_);
}

void Video::flush(bool next_sync) {
	if(sync_) {
		// If in sync, that takes priority. Output the proper amount of sync.
		crt_.output_sync(int(time_since_update_.as_integral()));
	} else {
		// If not presently in sync, then...

		if(line_data_) {
			// If there is output data queued, output it either if it's being interrupted by
			// sync, or if we're past its end anyway. Otherwise let it be.
			int data_length = static_cast<int>(line_data_pointer_ - line_data_);
			if(data_length < int(time_since_update_.as_integral()) || next_sync) {
				auto output_length = std::min(data_length, int(time_since_update_.as_integral()));
				crt_.output_data(output_length);
				line_data_pointer_ = line_data_ = nullptr;
				time_since_update_ -= HalfCycles(output_length);
			} else return;
		}

		// Any pending pixels being dealt with, pad with the white level.
		uint8_t *colour_pointer = static_cast<uint8_t *>(crt_.begin_data(1));
		if(colour_pointer) *colour_pointer = 0xff;
		crt_.output_level(int(time_since_update_.as_integral()));
	}

	time_since_update_ = 0;
}

void Video::set_sync(bool sync) {
	// Do nothing if sync hasn't changed.
	if(sync_ == sync) return;

	// Complete whatever was being drawn, and update sync.
	flush(sync);
	sync_ = sync;
}

void Video::output_byte(uint8_t byte) {
	// Complete whatever was going on.
	if(sync_) return;
	flush();

	// Grab a buffer if one isn't already available.
	if(!line_data_) {
		line_data_pointer_ = line_data_ = crt_.begin_data(StandardAllocationSize);
	}

	// If a buffer was obtained, serialise the new pixels.
	if(line_data_) {
		// If the buffer is full, output it now and obtain a new one
		if(line_data_pointer_ - line_data_ == StandardAllocationSize) {
			crt_.output_data(StandardAllocationSize, StandardAllocationSize);
			time_since_update_ -= StandardAllocationSize;
			line_data_pointer_ = line_data_ = crt_.begin_data(StandardAllocationSize);
			if(!line_data_) return;
		}

		// Convert to one-byte-per-pixel where any non-zero value will act as white.
		uint8_t mask = 0x80;
		for(int c = 0; c < 8; c++) {
			line_data_pointer_[c] = byte & mask;
			mask >>= 1;
		}
		line_data_pointer_ += 8;
	}
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Video::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / 0.5f;
}
