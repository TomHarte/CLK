//
//  MassStorageDevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MassStorageDevice_hpp
#define MassStorageDevice_hpp

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Storage {
namespace MassStorage {

/*!
	A mass storage device is usually:

		* large;
		* fixed; and
		* part of a class with a very wide array of potential speeds and timings.

	Within this emulator, mass storage devices don't attempt to emulate
	any specific medium, they just offer block-based access to a
	linearly-addressed store.
*/
class MassStorageDevice {
	public:
		virtual ~MassStorageDevice() {}

		/*!
			@returns The size of each individual block.
		*/
		virtual size_t get_block_size() = 0;

		/*!
			Block addresses run from 0 to n. The total number of blocks, n,
			therefore provides the range of valid addresses.

			@returns The total number of blocks on the device.
		*/
		virtual size_t get_number_of_blocks() = 0;

		/*!
			@returns The current contents of the block at @c address.
		*/
		virtual std::vector<uint8_t> get_block(size_t address) = 0;

		/*!
			Sets new contents for the block at @c address.
		*/
		virtual void set_block([[maybe_unused]] size_t address, const std::vector<uint8_t> &) {}
};

}
}

#endif /* MassStorageDevice_hpp */
