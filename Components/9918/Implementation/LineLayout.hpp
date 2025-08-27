//
//  LineLayout.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/05/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace TI::TMS {

template <Personality personality, typename Enable = void> struct LineLayout;

//	Line layout is:
//
//	[0, EndOfSync]							sync
//	(EndOfSync, StartOfColourBurst]			blank
//	(StartOfColourBurst, EndOfColourBurst]	colour burst
//	(EndOfColourBurst, EndOfLeftErase]		blank
//	(EndOfLeftErase, EndOfLeftBorder]		border colour
//	(EndOfLeftBorder, EndOfPixels]			pixel content
//	(EndOfPixels, EndOfRightBorder]			border colour
//	[EndOfRightBorder, <end of line>]		blank
//
//	... with minor caveats:
//		* horizontal adjust on the Yamaha VDPs is applied to EndOfLeftBorder and EndOfPixels;
//		* the Sega VDPs may programatically extend the left border; and
//		* text mode on all VDPs adjusts border width.

template <Personality personality> struct LineLayout<personality, std::enable_if_t<is_classic_vdp(personality)>> {
	static constexpr int StartOfSync		= 0;
	static constexpr int EndOfSync			= 26;
	static constexpr int StartOfColourBurst	= 29;
	static constexpr int EndOfColourBurst	= 43;
	static constexpr int EndOfLeftErase		= 50;
	static constexpr int EndOfLeftBorder	= 63;
	static constexpr int EndOfPixels		= 319;
	static constexpr int EndOfRightBorder	= 334;

	static constexpr int CyclesPerLine		= 342;

	static constexpr int TextModeEndOfLeftBorder	= 69;
	static constexpr int TextModeEndOfPixels		= 309;

	static constexpr int ModeLatchCycle		= 36;	// Just a guess; correlates with the known 144 for the Yamaha VDPs,
													// and falls into the collection gap between the final sprite
													// graphics and the initial tiles or pixels.

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	static constexpr int VRAMAccessDelay = 6;
};

template <Personality personality> struct LineLayout<personality, std::enable_if_t<is_yamaha_vdp(personality)>> {
	static constexpr int StartOfSync		= 0;
	static constexpr int EndOfSync			= 100;
	static constexpr int StartOfColourBurst	= 113;
	static constexpr int EndOfColourBurst	= 167;
	static constexpr int EndOfLeftErase		= 202;
	static constexpr int EndOfLeftBorder	= 258;
	static constexpr int EndOfPixels		= 1282;
	static constexpr int EndOfRightBorder	= 1341;

	static constexpr int CyclesPerLine		= 1368;

	static constexpr int TextModeEndOfLeftBorder	= 294;
	static constexpr int TextModeEndOfPixels		= 1254;

	static constexpr int ModeLatchCycle		= 144;

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	static constexpr int VRAMAccessDelay = 16;
};

}
