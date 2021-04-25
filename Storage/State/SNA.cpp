//
//  SNA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "SNA.hpp"

using namespace Storage::State;

std::unique_ptr<Analyser::Static::Target> SNA::load(const std::string &file_name) {
	// 0x1a byte header:
	//
	//	00	I
	//	01	HL'
	//	03	DE'
	//	05	BC'
	//	07	AF'
	//	09	HL
	//	0B	DE
	//	0D	BC
	//	0F	IY
	//	11	IX
	//	13	IFF2 (in bit 2)
	//	14	R
	//	15	AF
	//	17	SP
	//	19	interrupt mode
	//	1A	border colour
	//	1B–	48kb RAM contents
	//
	// (perform a POP to get the PC)

	(void)file_name;
	return nullptr;
}
