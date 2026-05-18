//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

using namespace Tandy::CoCo;

DiskController::DiskController() : WD::WD1770(P1773) {
	emplace_drives(2, 8'000'000, 300, 2);
}

void DiskController::set_control(uint8_t) {
	// TODO:
	//
	//	b7: halt flag, 1 = enabled
	//	b6: drive select 3
	//	b5: density, 1 = double
	//	b4: write precompensation, 1 = enable
	//	b3: drive motors, 1 = on
	//	b0–b2: drive selects
}
