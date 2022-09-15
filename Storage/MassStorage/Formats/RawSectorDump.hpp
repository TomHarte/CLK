//
//  RawSectorDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef RawSectorDump_h
#define RawSectorDump_h

#include "../MassStorageDevice.hpp"
#include "../../FileHolder.hpp"

#include <cassert>

namespace Storage {
namespace MassStorage {

template <long sector_size> class RawSectorDump: public MassStorageDevice {
	public:
		RawSectorDump(const std::string &file_name, long offset = 0, long length = -1) :
			file_(file_name),
			file_size_((length == -1) ? long(file_.stats().st_size) : length),
			file_start_(offset)
		{
			// Is the file a multiple of sector_size bytes in size?
			if(file_size_ % sector_size) throw std::exception();
		}

		/* MassStorageDevices overrides. */
		size_t get_block_size() final {
			return sector_size;
		}

		size_t get_number_of_blocks() final {
			return size_t(file_size_ / sector_size);
		}

		std::vector<uint8_t> get_block(size_t address) final {
			file_.seek(file_start_ + long(address * sector_size), SEEK_SET);
			return file_.read(sector_size);
		}

		void set_block(size_t address, const std::vector<uint8_t> &contents) final {
			assert(contents.size() == sector_size);
			file_.seek(file_start_ + long(address * sector_size), SEEK_SET);
			file_.write(contents);
		}

	private:
		FileHolder file_;
		const long file_size_, file_start_;
};

}
}

#endif /* RawSectorDump_h */
