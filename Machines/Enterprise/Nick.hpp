//
//  Nick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Nick_hpp
#define Nick_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Outputs/CRT/CRT.hpp"

namespace Enterprise {

class Nick {
	public:
		Nick(const uint8_t *ram);

		void write(uint16_t address, uint8_t value);
		uint8_t read(uint16_t address);

		void run_for(HalfCycles);

		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

	private:
		Outputs::CRT::CRT crt_;
		const uint8_t *const ram_;

		// CPU-provided state.
		uint8_t line_parameter_control_ = 0xc0;
		uint16_t line_parameter_base_ = 0x0000;
		uint16_t border_colour_ = 0;

		// Ephemerals, related to current video position.
		int horizontal_counter_ = 0;
		uint16_t line_parameter_pointer_ = 0x0000;
		uint8_t line_parameters_[16];
		bool should_reload_line_parameters_ = false;
		uint16_t line_data_pointer_[2];

		// Current mode line parameters.
		uint8_t lines_remaining_ = 0x00;
		int left_margin_ = 0, right_margin_ = 0;
		enum class Mode {
			Vsync,
			Pixel,
			Attr,
			CH256,
			CH128,
			CH64,
			Unused,
			LPixel,
		} mode_ = Mode::Vsync;
		enum class State {
			Sync,
			Border,
			Pixels,
			Blank,
		} state_ = State::Sync;

		// An accumulator for border output regions.
		int border_duration_ = 0;
		void flush_border();

		// The destination for new pixels.
		static constexpr int allocation_size = 320;
		uint16_t *pixel_pointer_ = nullptr, *allocated_pointer_ = nullptr;
		int pixel_duration_ = 0;
		void flush_pixels();
};


}

#endif /* Nick_hpp */
