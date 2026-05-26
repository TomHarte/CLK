//
//  Resettable.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

namespace MachineTypes {

struct SoftResettable {
	virtual void soft_reset() = 0;
};

struct HardResettable {
	virtual void hard_reset() = 0;
};

}
