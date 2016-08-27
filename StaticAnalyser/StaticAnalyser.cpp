//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include <cstdlib>

using namespace StaticAnalyser;

std::list<Target> StaticAnalyser::GetTargets(const char *file_name)
{
	std::list<Target> targets;

	// Get the extension, if any; it will be assumed that extensions are reliable, so an extension is a broad-phase
	// test as to file format.
	const char *mixed_case_extension = strrchr(file_name, '.');
	char *lowercase_extension = nullptr;
	if(mixed_case_extension)
	{
		lowercase_extension = strdup(mixed_case_extension);
		char *parser = lowercase_extension;
		while(*parser)
		{
			*parser = (char)tolower(*parser);
			parser++;
		}
	}

	// Collect all disks, tapes and ROMs as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	std::list<std::shared_ptr<Storage::Disk>> disks;
	std::list<std::shared_ptr<Storage::Tape>> tapes;

	// Obtain the union of all platforms that

	printf("Lowercase extension: %s", lowercase_extension);

	free(lowercase_extension);
	return targets;
}
