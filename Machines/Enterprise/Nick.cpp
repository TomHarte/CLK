//
//  Nick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Nick.hpp"

#include <cstdio>

using namespace Enterprise;

Nick::Nick() :
	crt_(16 * 57, 16, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4) {

	// Just use RGB for now.
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
}

void Nick::write(uint16_t address, uint8_t value) {
	switch(address & 3) {
		default:
			printf("Unhandled Nick write: %02x -> %d\n", value, address & 3);
		break;
	}
}

uint8_t Nick::read([[maybe_unused]] uint16_t address) {
	return 0xff;
}

void Nick::run_for(HalfCycles) {
}

// MARK: - CRT passthroughs.

void Nick::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Nick::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}
