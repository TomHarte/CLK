//
//  MacintoshVolume.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MacintoshVolume_hpp
#define MacintoshVolume_hpp

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vector>

namespace Storage {
namespace MassStorage {
namespace Encodings {
namespace Macintosh {

enum class DriveType {
	SCSI
};

/*!
	On the Macintosh life is slightly complicated by Apple's
	decision to include device drivers on mass storage drives
	themselves — therefore a mass-storage device that is
	connected by SCSI will have different preliminary data on it
	than the same volume connected by ATA or as an HD20.

	Mass storage devices that respond to @c Volume can be made
	to provide the proper whole-volume encoding necessary to
	impersonate different types of Macintosh drive.
*/
class Volume {
	public:
		virtual void set_drive_type(DriveType type) = 0;
};

/*!
	A Mapper can used by a mass-storage device that knows the
	contents of an HFS partition to provide the conversion
	necessary to a particular type of drive.
*/
class Mapper {
	public:
		/*!
			Sets the drive type to map to and the number of blocks in the underlying partition.
		*/
		void set_drive_type(DriveType, size_t number_of_blocks);

		/*!
			Maps from a mass-storage device address to an address
			in the underlying [H/M]FS partition.
		*/
		ssize_t to_source_address(size_t address);

		/*!
			Converts from a source data block to one properly encoded for the drive type.

			Expected usage:

				const size_t source_address = mapper.to_source_address(unit_address);
				if(is_in_range_for_partition(source_address)) {
					return mapper.convert_source_block(source_address, get_block_contents(source_address));
				} else {
					return mapper.convert_source_block(source_address);
				}
		*/
		std::vector<uint8_t> convert_source_block(ssize_t source_address, std::vector<uint8_t> source_data = {});

		/*!
			@returns The total number of blocks on the entire volume.
		*/
		size_t get_number_of_blocks();

	private:
		DriveType drive_type_;
		size_t number_of_blocks_;
};

}
}
}
}

#endif /* MacintoshVolume_hpp */
