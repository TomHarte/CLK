//
//  Commodore.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Commodore.hpp"

std::wstring Storage::Data::Commodore::petscii_from_bytes(const uint8_t *string, int length, bool shifted) {
	std::wstring result;

	wchar_t unshifted_characters[256] = {
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u000d', u'\u0000', u'\u0000',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0008', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000',
		u'\u0020', u'\u0021', u'\u0022', u'\u0023', u'\u0024', u'\u0025', u'\u0026', u'\u0027', u'\u0028', u'\u0029', u'\u002a', u'\u002b', u'\u002c', u'\u002d', u'\u002e', u'\u002f',
		u'\u0030', u'\u0031', u'\u0032', u'\u0033', u'\u0034', u'\u0035', u'\u0036', u'\u0037', u'\u0038', u'\u0039', u'\u0022', u'\u003b', u'\u003c', u'\u003d', u'\u003e', u'\u003f',
		u'\u0040', u'\u0041', u'\u0042', u'\u0043', u'\u0044', u'\u0045', u'\u0046', u'\u0047', u'\u0048', u'\u0049', u'\u004a', u'\u004b', u'\u004c', u'\u004d', u'\u004e', u'\u004f',
		u'\u0050', u'\u0051', u'\u0052', u'\u0053', u'\u0054', u'\u0055', u'\u0056', u'\u0057', u'\u0058', u'\u0059', u'\u005a', u'\u005b', u'\u00a3', u'\u005d', u'\u2191', u'\u2190',
		u'\u2500', u'\u2660', u'\u2502', u'\u2500', u'\ufffd', u'\ufffd', u'\ufffd', u'\ufffd', u'\ufffd', u'\u256e', u'\u2570', u'\u256f', u'\ufffd', u'\u2572', u'\u2571', u'\ufffd',
		u'\ufffd', u'\u25cf', u'\ufffd', u'\u2665', u'\ufffd', u'\u256d', u'\u2573', u'\u25cb', u'\u2663', u'\ufffd', u'\u2666', u'\u253c', u'\ufffd', u'\u2502', u'\u03c0', u'\u25e5',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u000d', u'\u0000', u'\u0000',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0008', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000',
		u'\u0020', u'\u258c', u'\u2584', u'\u2594', u'\u2581', u'\u258f', u'\u2592', u'\u2595', u'\ufffd', u'\u25e4', u'\ufffd', u'\u251c', u'\u2597', u'\u2514', u'\u2510', u'\u2582',
		u'\u250c', u'\u2534', u'\u252c', u'\u2524', u'\u258e', u'\u258d', u'\ufffd', u'\ufffd', u'\ufffd', u'\u2583', u'\ufffd', u'\u2596', u'\u259d', u'\u2518', u'\u2598', u'\u259a',
		u'\u2500', u'\u2660', u'\u2502', u'\u2500', u'\ufffd', u'\ufffd', u'\ufffd', u'\ufffd', u'\ufffd', u'\u256e', u'\u2570', u'\u256f', u'\ufffd', u'\u2572', u'\u2571', u'\ufffd',
		u'\ufffd', u'\u25cf', u'\ufffd', u'\u2665', u'\ufffd', u'\u256d', u'\u2573', u'\u25cb', u'\u2663', u'\ufffd', u'\u2666', u'\u253c', u'\ufffd', u'\u2502', u'\u03c0', u'\u25e5',
		u'\u0020', u'\u258c', u'\u2584', u'\u2594', u'\u2581', u'\u258f', u'\u2592', u'\u2595', u'\ufffd', u'\u25e4', u'\ufffd', u'\u251c', u'\u2597', u'\u2514', u'\u2510', u'\u2582',
		u'\u250c', u'\u2534', u'\u252c', u'\u2524', u'\u258e', u'\u258d', u'\ufffd', u'\ufffd', u'\ufffd', u'\u2583', u'\ufffd', u'\u2596', u'\u259d', u'\u2518', u'\u2598', u'\u03c0'
	};

	wchar_t shifted_characters[256] = {
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u000d', u'\u0000', u'\u0000',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0008', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000',
		u'\u0020', u'\u0021', u'\u0022', u'\u0023', u'\u0024', u'\u0025', u'\u0026', u'\u0027', u'\u0028', u'\u0029', u'\u002a', u'\u002b', u'\u002c', u'\u002d', u'\u002e', u'\u002f',
		u'\u0030', u'\u0031', u'\u0032', u'\u0033', u'\u0034', u'\u0035', u'\u0036', u'\u0037', u'\u0038', u'\u0039', u'\u0022', u'\u003b', u'\u003c', u'\u003d', u'\u003e', u'\u003f',
		u'\u0040', u'\u0061', u'\u0062', u'\u0063', u'\u0064', u'\u0065', u'\u0066', u'\u0067', u'\u0068', u'\u0069', u'\u006a', u'\u006b', u'\u006c', u'\u006d', u'\u006e', u'\u006f',
		u'\u0070', u'\u0071', u'\u0072', u'\u0073', u'\u0074', u'\u0075', u'\u0076', u'\u0077', u'\u0078', u'\u0079', u'\u007a', u'\u005b', u'\u00a3', u'\u005d', u'\u2191', u'\u2190',
		u'\u2500', u'\u0041', u'\u0042', u'\u0043', u'\u0044', u'\u0045', u'\u0046', u'\u0047', u'\u0048', u'\u0049', u'\u004a', u'\u004b', u'\u004c', u'\u004d', u'\u004e', u'\u004f',
		u'\u0050', u'\u0051', u'\u0052', u'\u0053', u'\u0054', u'\u0055', u'\u0056', u'\u0057', u'\u0058', u'\u0059', u'\u005a', u'\u253c', u'\ufffd', u'\u2502', u'\u2592', u'\u25e5',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u000d', u'\u0000', u'\u0000',
		u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0008', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000', u'\u0000',
		u'\u0020', u'\u258c', u'\u2584', u'\u2594', u'\u2581', u'\u258f', u'\u2592', u'\u2595', u'\ufffd', u'\ufffd', u'\ufffd', u'\u251c', u'\u2597', u'\u2514', u'\u2510', u'\u2582',
		u'\u250c', u'\u2534', u'\u252c', u'\u2524', u'\u258e', u'\u258d', u'\ufffd', u'\ufffd', u'\ufffd', u'\u2583', u'\u2713', u'\u2596', u'\u259d', u'\u2518', u'\u2598', u'\u259a',
		u'\u2500', u'\u0041', u'\u0042', u'\u0043', u'\u0044', u'\u0045', u'\u0046', u'\u0047', u'\u0048', u'\u0049', u'\u004a', u'\u004b', u'\u004c', u'\u004d', u'\u004e', u'\u004f',
		u'\u0050', u'\u0051', u'\u0052', u'\u0053', u'\u0054', u'\u0055', u'\u0056', u'\u0057', u'\u0058', u'\u0059', u'\u005a', u'\u253c', u'\ufffd', u'\u2502', u'\u2592', u'\ufffd',
		u'\u0020', u'\u258c', u'\u2584', u'\u2594', u'\u2581', u'\u258f', u'\u2592', u'\u2595', u'\ufffd', u'\ufffd', u'\ufffd', u'\u251c', u'\u2597', u'\u2514', u'\u2510', u'\u2582',
		u'\u250c', u'\u2534', u'\u252c', u'\u2524', u'\u258e', u'\u258d', u'\ufffd', u'\ufffd', u'\ufffd', u'\u2583', u'\u2713', u'\u2596', u'\u259d', u'\u2518', u'\u2598', u'\u2592'
	};

	wchar_t *table = shifted ? shifted_characters : unshifted_characters;
	for(int c = 0; c < length; c++) {
		wchar_t next_character = table[string[c]];
		if(next_character) result.push_back(next_character);
	}

	return result;
}
