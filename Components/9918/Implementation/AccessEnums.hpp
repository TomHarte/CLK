//
//  AccessEnums.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef AccessEnums_hpp
#define AccessEnums_hpp


namespace TI {
namespace TMS {

// The screen mode is a necessary predecessor to picking the line mode,
// which is the thing latched per line.
enum class ScreenMode {
	// Original TMS modes.
	Blank,
	Text,
	MultiColour,
	ColouredText,
	Graphics,

	// 8-bit Sega modes.
	SMSMode4,

	// New Yamaha V9938 modes.
	YamahaText80,
	YamahaGraphics3,
	YamahaGraphics4,
	YamahaGraphics5,
	YamahaGraphics6,
	YamahaGraphics7,

	// Rebranded Yamaha V9938 modes.
	YamahaGraphics1 = ColouredText,
	YamahaGraphics2 = Graphics,
};

enum class LineMode {
	Text,
	Character,
	Refresh,
	SMS,
	Yamaha,
};

enum class MemoryAccess {
	Read, Write, None
};

}
}

#endif /* AccessEnums_hpp */
