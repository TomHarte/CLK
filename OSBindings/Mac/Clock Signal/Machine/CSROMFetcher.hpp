//
//  ROMFetcher.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "ROMMachine.hpp"

/// @returns a ROM fetcher that will check: (i) this application's support directory; and
/// (ii) this application's bundle for any requested ROMs, in that order.
///
/// All ROMs are found to be missing when the ROM fetcher is used will be added
/// to @c missing.
ROMMachine::ROMFetcher CSROMFetcher(ROM::Request *missing = nullptr);

/// Loads the binary file located at @c url and then tests for whether it matches anything
/// known to the ROM catalogue. If so then a copy of the ROM will be retained where it
/// can later be found by the ROM fetcher returned by @c CSROMFetcher.
///
/// @returns @c true if the file was loaded successfully and matches something in
/// the library; @c false otherwise.
BOOL CSInstallROM(NSURL *url);
