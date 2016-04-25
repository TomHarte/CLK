//
//  CRTTypes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTTypes_h
#define CRTTypes_h

namespace Outputs {
namespace CRT {

struct Rect {
	struct {
		float x, y;
	} origin;

	struct {
		float width, height;
	} size;

	Rect() {}
	Rect(float x, float y, float width, float height) :
		origin({.x = x, .y = y}), size({.width = width, .height = height}) {}
};

enum DisplayType {
	PAL50,
	NTSC60
};

enum ColourSpace {
	YIQ,
	YUV
};

enum OutputDevice {
	Monitor,
	Television
};

}
}

#endif /* CRTTypes_h */
