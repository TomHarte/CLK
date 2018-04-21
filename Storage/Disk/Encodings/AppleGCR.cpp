//
//  AppleGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleGCR.hpp"

using namespace Storage::Encodings;

unsigned int AppleGCR::five_and_three_encoding_for_value(int value) {
	static const unsigned int values[] = {
		0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba,
		0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
		0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef,
		0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
	};
	return values[value & 0x1f];
}

void AppleGCR::encode_five_and_three_block(uint8_t *destination, uint8_t *source) {
	destination[0] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[0] >> 3 ));
	destination[1] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[0] << 2) | (source[1] >> 6) ));
	destination[2] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[1] >> 1 ));
	destination[3] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[1] << 4) | (source[2] >> 4) ));
	destination[4] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[2] << 1) | (source[3] >> 7) ));
	destination[5] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[3] >> 2 ));
	destination[6] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[3] << 3) | (source[4] >> 5) ));
	destination[7] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[4] ));
}

unsigned int AppleGCR::six_and_two_encoding_for_value(int value) {
	static const unsigned int values[] = {
		0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
		0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
		0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
		0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
		0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
		0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
		0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
		0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};
	return values[value & 0x3f];
}

void AppleGCR::encode_six_and_two_block(uint8_t *destination, uint8_t *source) {
	destination[0] = static_cast<uint8_t>(six_and_two_encoding_for_value( source[0] >> 2 ));
	destination[1] = static_cast<uint8_t>(six_and_two_encoding_for_value( (source[0] << 4) | (source[1] >> 4) ));
	destination[2] = static_cast<uint8_t>(six_and_two_encoding_for_value( (source[1] << 2) | (source[2] >> 6) ));
	destination[3] = static_cast<uint8_t>(six_and_two_encoding_for_value( source[2] ));
}
