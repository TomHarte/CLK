//
//  AcornADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef AcornADF_hpp
#define AcornADF_hpp

#include "../MassStorageDevice.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace MassStorage {

/*!
	Provides a @c MassStorageDevice containing an Acorn ADFS image, which is just a
	sector dump of an ADFS volume.
*/
class AcornADF: public MassStorageDevice {
	public:
		AcornADF(const std::string &file_name);

	private:
		FileHolder file_;

		/* MassStorageDevices overrides. */
		size_t get_block_size() final;
		size_t get_number_of_blocks() final;
		std::vector<uint8_t> get_block(size_t address) final;
		void set_block(size_t address, const std::vector<uint8_t> &) final;
};

}
}

#endif /* AcornADF_hpp */
