//
//  CommonAtrributes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <string>
#include <vector>

namespace Outputs::Display::OpenGL {

/*!
	The union of all named attributes used by shaders that consume Scans.
*/
inline std::vector<std::string> scan_attributes() {
	return std::vector<std::string>{
		"scanEndpoint0DataOffset",
		"scanEndpoint0CyclesSinceRetrace",
		"scanEndpoint0CompositeAngle",
		"scanEndpoint1DataOffset",
		"scanEndpoint1CyclesSinceRetrace",
		"scanEndpoint1CompositeAngle",
		"scanDataY",
		"scanLine",
		"scanCompositeAmplitude",
	};
};

/*!
	The union of all named attributes used by shaders that consume Dirtyones.
*/
inline std::vector<std::string> dirty_zone_attributes() {
	return std::vector<std::string>{
	};
}

}
