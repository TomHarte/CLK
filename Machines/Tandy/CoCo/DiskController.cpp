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
