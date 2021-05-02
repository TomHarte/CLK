//
//  DAT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "DAT.hpp"

using namespace Storage::MassStorage;

DAT::DAT(const std::string &file_name) : RawSectorDump(file_name) {
	// Does the third sector contain the 'Hugo' signature?
	const auto sector3 = get_block(2);
	if(sector3.size() != 256) {
		throw std::exception();
	}
	if(sector3[1] != 'H' || sector3[2] != 'u' || sector3[3] != 'g' || sector3[4] != 'o') {
		throw std::exception();
	}
}
