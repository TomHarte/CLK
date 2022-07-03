//
//  SoftwareScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/07/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

using namespace Outputs::Display::Software;

template <
	Outputs::Display::InputDataType input_type,
	Outputs::Display::DisplayType display_type,
	Outputs::Display::ColourSpace colour_space
> void ScanTarget::process() {
	// TODO.
}

void ScanTarget::set_modals(Modals m) {
	printf("");
}

void ScanTarget::submit() {
	printf("");
}
