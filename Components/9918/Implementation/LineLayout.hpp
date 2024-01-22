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
	constexpr static int StartOfSync		= 0;
	constexpr static int EndOfSync			= 26;
	constexpr static int StartOfColourBurst	= 29;
	constexpr static int EndOfColourBurst	= 43;
	constexpr static int EndOfLeftErase		= 50;
	constexpr static int EndOfLeftBorder	= 63;
	constexpr static int EndOfPixels		= 319;
	constexpr static int EndOfRightBorder	= 334;

	constexpr static int CyclesPerLine		= 342;

	constexpr static int TextModeEndOfLeftBorder	= 69;
	constexpr static int TextModeEndOfPixels		= 309;

	constexpr static int ModeLatchCycle		= 36;	// Just a guess; correlates with the known 144 for the Yamaha VDPs,
													// and falls into the collection gap between the final sprite
													// graphics and the initial tiles or pixels.

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	constexpr static int VRAMAccessDelay = 6;
};

template <Personality personality> struct LineLayout<personality, std::enable_if_t<is_yamaha_vdp(personality)>> {
	constexpr static int StartOfSync		= 0;
	constexpr static int EndOfSync			= 100;
	constexpr static int StartOfColourBurst	= 113;
	constexpr static int EndOfColourBurst	= 167;
	constexpr static int EndOfLeftErase		= 202;
	constexpr static int EndOfLeftBorder	= 258;
	constexpr static int EndOfPixels		= 1282;
	constexpr static int EndOfRightBorder	= 1341;

	constexpr static int CyclesPerLine		= 1368;

	constexpr static int TextModeEndOfLeftBorder	= 294;
	constexpr static int TextModeEndOfPixels		= 1254;

	constexpr static int ModeLatchCycle		= 144;

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	constexpr static int VRAMAccessDelay = 16;
};

}
