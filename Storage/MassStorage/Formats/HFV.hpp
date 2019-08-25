//
//  HFV.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef HFV_hpp
#define HFV_hpp

#include "../MassStorageDevice.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace MassStorage {

/*!
	Provides a @c MassStorageDevice containing an HFV image, which is a sector dump of
	a Macintosh drive that is not the correct size to be a floppy disk.
*/
class HFV: public MassStorageDevice {
	public:
		HFV(const std::string &file_name);

	private:
		FileHolder file_;

		size_t get_block_size() final;
		size_t get_number_of_blocks() final;
		std::vector<uint8_t> get_block(size_t address) final;

};

}
}

#endif /* HFV_hpp */
