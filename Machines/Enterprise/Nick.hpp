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
		Nick();

		void write(uint16_t address, uint8_t value);
		uint8_t read(uint16_t address);

		void run_for(HalfCycles);

		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

	private:
		Outputs::CRT::CRT crt_;
};


}

#endif /* Nick_hpp */
