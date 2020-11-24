//
//  2MG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "2MG.hpp"

using namespace Storage::Disk;

DiskImageHolderBase *Disk2MG::open(const std::string &file_name) {
	FileHolder file(file_name);

	// Check the signature.
	if(!file.check_signature("2IMG")) throw Error::InvalidFormat;

	// Skip the creator.
	file.seek(4, SEEK_CUR);

	// Grab the header size, version number and image format.
	const uint16_t header_size = file.get16le();
	const uint16_t version = file.get16le();
	const uint32_t format = file.get32le();
	const uint32_t flags = file.get32le();

	// Skip the number of ProDOS blocks; this is surely implicit from the data size?
	file.seek(4, SEEK_CUR);

	// Get the offset and size of the disk image data.
	const uint32_t data_start = file.get32le();
	const uint32_t data_size = file.get32le();

	// Skipped:
	//
	//	four bytes, offset to comment
	//	four bytes, length of comment
	//	four bytes, offset to creator-specific data
	//	four bytes, length of creator-specific data
	//
	// (all of which relate to optional appendages).

	// Validate.
	if(header_size < 0x40 || header_size >= file.stats().st_size) {
		throw Error::InvalidFormat;
	}
	if(version > 1) {
		throw Error::InvalidFormat;
	}

	// TODO: based on format, instantiate a suitable disk image.
	switch(format) {
		default: throw Error::InvalidFormat;
		case 0:
			// TODO: DOS 3.3 sector order.
		break;
		case 1:
			// TODO: ProDOS sector order.
		break;
		case 2:
			// TODO: NIB data (yuck!).
		break;
	}

	// So: maybe extend FileHolder to handle hiding the header before instantiating
	// a proper holder? Probably more valid than having each actual disk class do
	// its own range logic?
	(void)flags;
	(void)data_start;
	(void)data_size;

	throw Error::InvalidFormat;
	return nullptr;
}
