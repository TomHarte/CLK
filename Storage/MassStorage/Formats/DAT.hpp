//
//  DAT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef MassStorage_DAT_hpp
#define MassStorage_DAT_hpp

#include "../MassStorageDevice.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace MassStorage {

/*!
	Provides a @c MassStorageDevice containing an Acorn ADFS image, which is just a
	sector dump of an ADFS volume. It will be validated for an ADFS catalogue and communicate
	in 256-byte blocks.
*/
class DAT: public MassStorageDevice {
	public:
		DAT(const std::string &file_name);

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

#endif /* MassStorage_DAT_hpp */
