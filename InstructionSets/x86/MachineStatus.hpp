//
//  MachineStatus.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

namespace InstructionSet::x86 {

enum MachineStatus: uint16_t {
	//
	// 80286 state.
	//
	ProtectedModeEnable = 1 << 0,
	MonitorProcessorExtension = 1 << 1,
	EmulateProcessorExtension = 1 << 2,
	TaskSwitched = 1 << 3,
};

}

