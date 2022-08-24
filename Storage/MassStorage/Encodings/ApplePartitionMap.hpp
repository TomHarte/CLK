//
//  ApplePartitionMap.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef ApplePartitionMap_hpp
#define ApplePartitionMap_hpp

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/types.h>
#include <vector>

namespace Storage {
namespace MassStorage {
namespace Encodings {
namespace Apple {

enum class DriveType {
	SCSI
};

/*!
	Implements a device to volume mapping with an Apple Partition Map.

	The @c VolumeProvider provides both the volume to be embedded into a device,
	and device driver information if applicable.
*/
template <typename VolumeProvider> class PartitionMap {
	public:
		/*!
			Sets the drive type to map to and the number of blocks in the underlying partition.
		*/
		void set_drive_type(DriveType drive_type, size_t number_of_blocks) {
			drive_type_ = drive_type;
			number_of_blocks_ = number_of_blocks;
		}

		/*!
			@returns The total number of blocks on the entire volume.
		*/
		size_t get_number_of_blocks() const {
			return
				number_of_blocks_ +				// Size of the volume.
				size_t(non_volume_blocks());	// Size of everything else.
		}

		/*!
			Maps from a mass-storage device address to an address
			in the underlying volume.
		*/
		ssize_t to_source_address(size_t address) const {
			// The embedded volume is always the last thing on the device.
			return ssize_t(address) - ssize_t(non_volume_blocks());
		}

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
		std::vector<uint8_t> convert_source_block(ssize_t source_address, std::vector<uint8_t> source_data = {}) {
			// Addresses greater than or equal to zero map to the actual disk image.
			if(source_address >= 0) return source_data;

			// Switch to mapping relative to 0, for personal sanity.
			source_address += non_volume_blocks();

			// Block 0 is the device descriptor, which lists the total number of blocks,
			// and provides an offset to the driver, if any.
			if(!source_address) {
				const uint32_t total_device_blocks = uint32_t(get_number_of_blocks());
				const auto driver_size = uint16_t(driver_block_size());
				const auto driver_offset = uint16_t(predriver_blocks());
				const uint8_t driver_count = driver_size > 0 ? 1 : 0;

				/* The driver descriptor. */
				std::vector<uint8_t> driver_description = {
					0x45, 0x52,		/* device signature */
					0x02, 0x00,		/* block size, in bytes */

					uint8_t(total_device_blocks >> 24),
					uint8_t(total_device_blocks >> 16),
					uint8_t(total_device_blocks >> 8),
					uint8_t(total_device_blocks),
									/* number of blocks on device */

					0x00, 0x01,		/* reserved (formerly: device type) */
					0x00, 0x01,		/* reserved (formerly: device ID) */
					0x00, 0x00,
					0x00, 0x00,		/* reserved ('sbData', no further explanation given) */

					0x00, driver_count,		/* number of device descriptor entries */
					0x00, 0x00,

					/* first device descriptor's starting block */
					uint8_t(driver_offset >> 8), uint8_t(driver_offset),

					/* size of device driver */
					uint8_t(driver_size >> 8), uint8_t(driver_size),

					0x00, 0x01,		/*
										More modern documentation: operating system (MacOS = 1)
										Inside Macintosh IV: system type (Mac Plus = 1)
									*/
				};
				driver_description.resize(512);

				return driver_description;
			}

			// Blocks 1 and 2 contain entries of the partition map; there's also possibly an entry
			// for the driver.
			if(source_address < 3 + volume_provider_.HasDriver) {
				struct Partition {
					const char *name, *type;
					uint32_t start_block, size;
					uint8_t status;
				} partitions[3] = {
					{
						volume_provider_.name(),
						volume_provider_.type(),
						uint32_t(non_volume_blocks()),
						uint32_t(number_of_blocks_),
						0xb7
					},
					{
						"Apple",
						"Apple_partition_map",
						0x01,
						uint32_t(predriver_blocks()) - 1,
						0x37
					},
					{
						"Macintosh",
						"Apple_Driver",
						uint32_t(predriver_blocks()),
						uint32_t(driver_block_size()),
						0x7f
					},
				};

				std::vector<uint8_t> partition(512);

				// Fill in the fixed fields.
				partition[0] = 'P';	partition[1] = 'M';	/* Signature. */
				partition[7] = 3;						/* Number of partitions. */

				const Partition &details = partitions[source_address-1];

				partition[8] = uint8_t(details.start_block >> 24);
				partition[9] = uint8_t(details.start_block >> 16);
				partition[10] = uint8_t(details.start_block >> 8);
				partition[11] = uint8_t(details.start_block);

				partition[84] = partition[12] = uint8_t(details.size >> 24);
				partition[85] = partition[13] = uint8_t(details.size >> 16);
				partition[86] = partition[14] = uint8_t(details.size >> 8);
				partition[87] = partition[15] = uint8_t(details.size);

				// 32 bytes are allocated for each of the following strings.
				memcpy(&partition[16], details.name, strlen(details.name));
				memcpy(&partition[48], details.type, strlen(details.type));

				partition[91] = details.status;

				// The third entry in this constructed partition map is the driver;
				// add some additional details.
				if constexpr (VolumeProvider::HasDriver) {
					if(source_address == 3) {
						const auto driver_size = uint16_t(volume_provider_.driver_size());
						const auto driver_checksum = uint16_t(volume_provider_.driver_checksum());

						/* Driver size in bytes. */
						partition[98] = uint8_t(driver_size >> 8);
						partition[99] = uint8_t(driver_size);

						/* Driver checksum. */
						partition[118] = uint8_t(driver_checksum >> 8);
						partition[119] = uint8_t(driver_checksum);

						/* Driver target processor. */
						const char *driver_target = volume_provider_.driver_target();
						memcpy(&partition[120], driver_target, strlen(driver_target));

						// Various non-zero values that Apple HD SC Tool wrote are below; they are
						// documented as reserved officially, so I don't know their meaning.
						partition[137] = 0x01;
						partition[138] = 0x06;
						partition[143] = 0x01;
						partition[147] = 0x02;
						partition[149] = 0x07;
					}
				}

				return partition;
			}

			if constexpr (VolumeProvider::HasDriver) {
				// The remainder of the non-volume area is the driver.
				if(source_address >= predriver_blocks() && source_address < non_volume_blocks()) {
					const uint8_t *driver = volume_provider_.driver();
					const auto offset = (source_address - predriver_blocks()) * 512;
					return std::vector<uint8_t>(&driver[offset], &driver[offset + 512]);
				}
			}

			// Default: return an empty block.
			return std::vector<uint8_t>(512);
		}

	private:
		DriveType drive_type_;
		size_t number_of_blocks_;

		VolumeProvider volume_provider_;

		ssize_t predriver_blocks() const {
			return
				0x40;					// Holding:
										//	(i) the driver descriptor;
										//	(ii) the partition table; and
										//	(iii) the partition entries.
		}

		ssize_t non_volume_blocks() const {
			return
				predriver_blocks() +
				driver_block_size();	// Size of device driver (if any).
		}

		ssize_t driver_block_size() const {
			if constexpr (VolumeProvider::HasDriver) {
				return (volume_provider_.driver_size() + 511) >> 9;
			} else {
				return 0;
			}
		}
};

}
}
}
}

#endif /* ApplePartitionMap_hpp */
