//
//  FileHolder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "FileHolder.hpp"

using namespace Storage;

FileHolder::~FileHolder()
{
	if(file_) fclose(file_);
}

FileHolder::FileHolder(const char *file_name) : file_(nullptr)
{
	stat(file_name, &file_stats_);
	file_ = fopen(file_name, "rb");
	if(!file_) throw ErrorCantOpen;
}
