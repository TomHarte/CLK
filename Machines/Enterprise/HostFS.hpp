//
//  HostFS.h
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Enterprise {

constexpr uint8_t hostfs_rom[] = {
	// Standard header.
	'E', 'X', 'O', 'S', '_', 'R', 'O', 'M',

	// Pointer to device chain.
	0x00, 0x00,

	//
};

}
