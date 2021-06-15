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

		// Ephemerals, related to current video position.
		int horizontal_counter_ = 0;
		uint16_t line_parameter_pointer_ = 0x0000;
		uint8_t line_parameters_[16];
		bool should_reload_line_parameters_ = false;

		// Current mode line parameters.
		uint8_t lines_remaining_ = 0x00;
};


}

#endif /* Nick_hpp */
