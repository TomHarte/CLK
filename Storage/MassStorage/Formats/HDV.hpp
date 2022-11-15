//
//  HDV.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/11/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef HDV_hpp
#define HDV_hpp

#include "../MassStorageDevice.hpp"
#include "../../FileHolder.hpp"
#include "../Encodings/AppleIIVolume.hpp"

#include <limits>
#include <vector>

namespace Storage {
namespace MassStorage {

/*!
	Provides a @c MassStorageDevice containing an HDV image, which is a sector dump of
	the ProDOS volume of an Apple II drive.
*/
class HDV: public MassStorageDevice {
	public:
		/*!
			Constructs an HDV with the contents of the file named @c file_name within
			the range given.

			Raises an exception if the file name doesn't appear to identify a valid
			Apple II mass storage image.
		*/
		HDV(const std::string &file_name, long start = 0, long size = std::numeric_limits<long>::max());

	private:
		FileHolder file_;
		long file_start_, image_size_;
		Storage::MassStorage::Encodings::AppleII::Mapper mapper_;

		/// @returns -1 if @c address is out of range; the offset into the file at which
		/// the block for @c address resides otherwise.
		long offset_for_block(ssize_t address);

		/* MassStorageDevices overrides. */
		size_t get_block_size() final;
		size_t get_number_of_blocks() final;
		std::vector<uint8_t> get_block(size_t address) final;
		void set_block(size_t address, const std::vector<uint8_t> &) final;
};

}
}

#endif /* HDV_hpp */
