//
//  File.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "File.hpp"

bool Analyser::Static::Commodore::File::is_basic() {
	// BASIC files are always relocatable (?)
	if(type != File::RelocatableProgram) return false;

	uint16_t line_address = starting_address;
	int line_number = -1;

	// decide whether this is a BASIC file based on the proposition that:
	//	(1) they're always relocatable; and
	//	(2) they have a per-line structure of:
	//		[4 bytes: address of start of next line]
	//		[4 bytes: this line number]
	//		... null-terminated code ...
	//	(with a next line address of 0000 indicating end of program)
	while(1) {
		if(size_t(line_address - starting_address) >= data.size() + 2) break;

		uint16_t next_line_address = data[line_address - starting_address];
		next_line_address |= data[line_address - starting_address + 1] << 8;

		if(!next_line_address) {
			return true;
		}
		if(next_line_address < line_address + 5) break;

		if(size_t(line_address - starting_address) >= data.size() + 5) break;
		uint16_t next_line_number = data[line_address - starting_address + 2];
		next_line_number |= data[line_address - starting_address + 3] << 8;

		if(next_line_number <= line_number) break;

		line_number = uint16_t(next_line_number);
		line_address = next_line_address;
	}

	return false;
}
