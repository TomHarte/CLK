//
//  AccessEnums.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef AccessEnums_hpp
#define AccessEnums_hpp


namespace TI::TMS {

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

constexpr int pixels_per_byte(ScreenMode mode) {
	switch(mode) {
		default:
		case ScreenMode::Blank:				return 1;
		case ScreenMode::Text:				return 6;
		case ScreenMode::MultiColour:		return 2;
		case ScreenMode::ColouredText:		return 8;
		case ScreenMode::Graphics:			return 8;
		case ScreenMode::SMSMode4:			return 2;
		case ScreenMode::YamahaText80:		return 6;
		case ScreenMode::YamahaGraphics3:	return 8;
		case ScreenMode::YamahaGraphics4:	return 2;
		case ScreenMode::YamahaGraphics5:	return 4;
		case ScreenMode::YamahaGraphics6:	return 2;
		case ScreenMode::YamahaGraphics7:	return 1;
	}
}

constexpr int width(ScreenMode mode) {
	switch(mode) {
		default:
		case ScreenMode::Blank:				return 0;
		case ScreenMode::Text:				return 240;
		case ScreenMode::MultiColour:		return 256;
		case ScreenMode::ColouredText:		return 256;
		case ScreenMode::Graphics:			return 256;
		case ScreenMode::SMSMode4:			return 256;
		case ScreenMode::YamahaText80:		return 480;
		case ScreenMode::YamahaGraphics3:	return 256;
		case ScreenMode::YamahaGraphics4:	return 256;
		case ScreenMode::YamahaGraphics5:	return 512;
		case ScreenMode::YamahaGraphics6:	return 512;
		case ScreenMode::YamahaGraphics7:	return 256;
	}
}

constexpr bool interleaves_banks(ScreenMode mode) {
	return mode == ScreenMode::YamahaGraphics6 || mode == ScreenMode::YamahaGraphics7;
}


enum class FetchMode {
	Text,
	Character,
	Refresh,
	SMS,
	Yamaha,
};

enum class MemoryAccess {
	Read, Write, None
};

enum class VerticalState {
	/// Describes any line on which pixels do not appear and no fetching occurs, including
	/// the border, blanking and sync.
	Blank,
	/// A line on which pixels do not appear but fetching occurs.
	Prefetch,
	/// A line on which pixels appear and fetching occurs.
	Pixels,
};

enum class SpriteMode {
	Mode1,
	Mode2,
	MasterSystem,
};

}

#endif /* AccessEnums_hpp */
