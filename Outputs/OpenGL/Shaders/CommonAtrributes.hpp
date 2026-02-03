//
//  CommonAtrributes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <vector>

namespace Outputs::Display::OpenGL {

/*!
	The named attributes shared by the union of all shaders that consume Scans.
*/
inline std::vector<std::string> scan_attributes() {
	return std::vector<std::string>{
		"scanEndpoint0DataOffset",
		"scanEndpoint1DataOffset",
		"scanDataY"
	};
};

}
