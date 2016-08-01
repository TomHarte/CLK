//
//  D64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "D64.hpp"

using namespace Storage;

D64::D64(const char *file_name)
{
	throw ErrorNotD64;
}

D64::~D64()
{
	if(_file) fclose(_file);
}

unsigned int D64::get_head_position_count()
{
	return 0;
}

std::shared_ptr<Track> D64::get_track_at_position(unsigned int position)
{
	return std::shared_ptr<Track>();
}
