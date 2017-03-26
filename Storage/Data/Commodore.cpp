//
//  Commodore.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Commodore.hpp"

std::wstring Storage::Data::Commodore::petscii_from_bytes(const uint8_t *string, int length, bool shifted) {
	std::wstring result;

	wchar_t unshifted_characters[256] = {
		L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\r',	L'\0',	L'\0',	
		L'\0',	L'\0',	L'\0',	L'\0',	L'\b',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',
		L' ',	L'!',	L'"',	L'#',	L'$',	L'%',	L'&',	L'\'',	L'(',	L')',	L'*',	L'+',	L',',	L'-',	L'.',	L'/',
		L'0',	L'1',	L'2',	L'3',	L'4',	L'5',	L'6',	L'7',	L'8',	L'9',	L'"',	L';',	L'<',	L'=',	L'>',	L'?',
		L'@',	L'A',	L'B',	L'C',	L'D',	L'E',	L'F',	L'G',	L'H',	L'I',	L'J',	L'K',	L'L',	L'M',	L'N',	L'O',
		L'P',	L'Q',	L'R',	L'S',	L'T',	L'U',	L'V',	L'W',	L'X',	L'Y',	L'Z',	L'[',	L'£',	L']',	L'↑',	L'←',
		L'─',	L'♠',	L'│',	L'─',	L'�',	L'�',	L'�',	L'�',	L'�',	L'╮',	L'╰',	L'╯',	L'�',	L'╲',	L'╱',	L'�',
		L'�',	L'●',	L'�',	L'♥',	L'�',	L'╭',	L'╳',	L'○',	L'♣',	L'�',	L'♦',	L'┼',	L'�',	L'│',	L'π',	L'◥',
		L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\r',	L'\0',	L'\0',
		L'\0',	L'\0',	L'\0',	L'\0',	L'\b',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',
		L' ',	L'▌',	L'▄',	L'▔',	L'▁',	L'▏',	L'▒',	L'▕',	L'�',	L'◤',	L'�',	L'├',	L'▗',	L'└',	L'┐',	L'▂',
		L'┌',	L'┴',	L'┬',	L'┤',	L'▎',	L'▍',	L'�',	L'�',	L'�',	L'▃',	L'�',	L'▖',	L'▝',	L'┘',	L'▘',	L'▚',
		L'─',	L'♠',	L'│',	L'─',	L'�',	L'�',	L'�',	L'�',	L'�',	L'╮',	L'╰',	L'╯',	L'�',	L'╲',	L'╱',	L'�',
		L'�',	L'●',	L'�',	L'♥',	L'�',	L'╭',	L'╳',	L'○',	L'♣',	L'�',	L'♦',	L'┼',	L'�',	L'│',	L'π',	L'◥',
		L' ',	L'▌',	L'▄',	L'▔',	L'▁',	L'▏',	L'▒',	L'▕',	L'�',	L'◤',	L'�',	L'├',	L'▗',	L'└',	L'┐',	L'▂',
		L'┌',	L'┴',	L'┬',	L'┤',	L'▎',	L'▍',	L'�',	L'�',	L'�',	L'▃',	L'�',	L'▖',	L'▝',	L'┘',	L'▘',	L'π',
	};

	wchar_t shifted_characters[256] = {
		L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\r',	L'\0',	L'\0',	
		L'\0',	L'\0',	L'\0',	L'\0',	L'\b',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',
		L' ',	L'!',	L'"',	L'#',	L'$',	L'%',	L'&',	L'\'',	L'(',	L')',	L'*',	L'+',	L',',	L'-',	L'.',	L'/',
		L'0',	L'1',	L'2',	L'3',	L'4',	L'5',	L'6',	L'7',	L'8',	L'9',	L'"',	L';',	L'<',	L'=',	L'>',	L'?',
		L'@',	L'a',	L'b',	L'c',	L'd',	L'e',	L'f',	L'g',	L'h',	L'i',	L'j',	L'k',	L'l',	L'm',	L'n',	L'o',
		L'p',	L'q',	L'r',	L's',	L't',	L'u',	L'v',	L'w',	L'x',	L'y',	L'z',	L'[',	L'£',	L']',	L'↑',	L'←',
		L'─',	L'A',	L'B',	L'C',	L'D',	L'E',	L'F',	L'G',	L'H',	L'I',	L'J',	L'K',	L'L',	L'M',	L'N',	L'O',
		L'P',	L'Q',	L'R',	L'S',	L'T',	L'U',	L'V',	L'W',	L'X',	L'Y',	L'Z',	L'┼',	L'�',	L'│',	L'▒',	L'◥',
		L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\r',	L'\0',	L'\0',
		L'\0',	L'\0',	L'\0',	L'\0',	L'\b',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',	L'\0',
		L' ',	L'▌',	L'▄',	L'▔',	L'▁',	L'▏',	L'▒',	L'▕',	L'�',	L'�',	L'�',	L'├',	L'▗',	L'└',	L'┐',	L'▂',
		L'┌',	L'┴',	L'┬',	L'┤',	L'▎',	L'▍',	L'�',	L'�',	L'�',	L'▃',	L'✓',	L'▖',	L'▝',	L'┘',	L'▘',	L'▚',
		L'─',	L'A',	L'B',	L'C',	L'D',	L'E',	L'F',	L'G',	L'H',	L'I',	L'J',	L'K',	L'L',	L'M',	L'N',	L'O',
		L'P',	L'Q',	L'R',	L'S',	L'T',	L'U',	L'V',	L'W',	L'X',	L'Y',	L'Z',	L'┼',	L'�',	L'│',	L'▒',	L'�',
		L' ',	L'▌',	L'▄',	L'▔',	L'▁',	L'▏',	L'▒',	L'▕',	L'�',	L'�',	L'�',	L'├',	L'▗',	L'└',	L'┐',	L'▂',
		L'┌',	L'┴',	L'┬',	L'┤',	L'▎',	L'▍',	L'�',	L'�',	L'�',	L'▃',	L'✓',	L'▖',	L'▝',	L'┘',	L'▘',	L'▒',
	};

	wchar_t *table = shifted ? shifted_characters : unshifted_characters;
	for(int c = 0; c < length; c++) {
		wchar_t next_character = table[string[c]];
		if(next_character) result.push_back(next_character);
	}

	return result;
}
