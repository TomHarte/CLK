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
		origin({x, y}), size({width, height}) {}
};

enum class DisplayType {
	PAL50,
	NTSC60
};

enum class ColourSpace {
	YIQ,
	YUV
};

enum class OutputDevice {
	Monitor,
	Television
};

}
}

#endif /* CRTTypes_h */
