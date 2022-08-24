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

#include "ApplePartitionMap.hpp"

namespace Storage {
namespace MassStorage {
namespace Encodings {
namespace Macintosh {

using DriveType = Storage::MassStorage::Encodings::Apple::DriveType;

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

struct VolumeProvider {
	static constexpr bool HasDriver = true;
	size_t driver_size() const;
	uint16_t driver_checksum() const;
	const uint8_t *driver() const;
	const char *driver_target() const;

	const char *name() const;
	const char *type() const;
};

/*!
	A Mapper can used by a mass-storage device that knows the
	contents of an HFS partition to provide the conversion
	necessary to a particular type of drive.
*/
using Mapper = Storage::MassStorage::Encodings::Apple::PartitionMap<VolumeProvider>;

}
}
}
}

#endif /* MacintoshVolume_hpp */
