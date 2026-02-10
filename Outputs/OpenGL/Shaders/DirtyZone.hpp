//
//  DirtyZone.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include <cstdint>

namespace Outputs::Display::OpenGL {

/*!
	Defines the top and bottom vertical extents of a rectangle;
	its horizontal bounds are not dynamic.
*/
struct DirtyZone {
	uint16_t begin;
	uint16_t end;
};

}
