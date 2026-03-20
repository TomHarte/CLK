//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "ClockReceiver/ClockReceiver.hpp"
#include "Outputs/CRT/CRT.hpp"

namespace Thomson::MO5 {

struct Video {
public:
	Video(const uint8_t *pixels, const uint8_t *attributes);
	void run_for(Cycles);
	Cycles next_sequence_point() const;

	void set_border_colour(uint8_t);
	bool irq() const;

	// MARK: - Standard boilerplate.

	void set_scan_target(Outputs::Display::ScanTarget *const target) {
		crt_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) {
		crt_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const {
		return crt_.get_display_type();
	}

private:
	const uint8_t *pixels_ = nullptr;
	const uint8_t *attributes_ = nullptr;
	Outputs::CRT::CRT crt_;
	int position_ = 0;

	uint16_t source_address_ = 0;
	uint16_t border_ = 0;
	uint16_t *output_ = nullptr;
};

}

#endif /* Video_hpp */
