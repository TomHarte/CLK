//
//  MacintoshVolume.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MacintoshVolume.hpp"

using namespace Storage::MassStorage::Encodings::Macintosh;

void Mapper::set_drive_type(DriveType drive_type, size_t number_of_blocks) {
	drive_type_ = drive_type;
	number_of_blocks_ = number_of_blocks;
}

size_t Mapper::get_number_of_blocks() {
	return number_of_blocks_ + 5;
}

ssize_t Mapper::to_source_address(size_t address) {
	/*
		Reserve one block at the start of the device
		for a partition map.

		TODO: and four or so more for a driver?
	*/
	return ssize_t(address) - 5;
}

std::vector<uint8_t> Mapper::convert_source_block(ssize_t source_address, std::vector<uint8_t> source_data) {
	switch(source_address) {
		case -5: {
			uint32_t total_device_blocks = uint32_t(number_of_blocks_ + 5);

			/* The driver descriptor. */
			std::vector<uint8_t> driver_description = {
				0x45, 0x52,		/* device signature */
				0x02, 0x00,		/* block size, in bytes */

				uint8_t(total_device_blocks >> 24),
				uint8_t(total_device_blocks >> 16),
				uint8_t(total_device_blocks >> 8),
				uint8_t(total_device_blocks),
								/* number of blocks on device */

				0x00, 0x00,		/* reserved (formerly: device type) */
				0x00, 0x00,		/* reserved (formerly: device ID) */
				0x00, 0x00,
				0x00, 0x00,		/* reserved ('sbData', no further explanation given) */

				0x00, 0x01,		/* number of device descriptor entries */
				0x00, 0x01,		/* first device descriptor's starting block */
				0x00, 0x04,		/* size of device driver */
				0x00, 0x01,		/*
									More modern documentation: operating system (MacOS = 1)
									Inside Macintosh IV: system type (Mac Plus = 1)
								*/
			};
			driver_description.resize(512);

			return driver_description;
		}

		case -4: case -3: case -2: case -1: /* TODO */
		return std::vector<uint8_t>(512);

		default: return source_data;
	}
}
