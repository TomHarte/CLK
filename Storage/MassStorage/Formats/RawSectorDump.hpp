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

namespace Storage {
namespace MassStorage {

template <size_t sector_size> class RawSectorDump: public MassStorageDevice {
	public:
		RawSectorDump(const std::string &file_name) : file_(file_name) {
			// Is the file a multiple of sector_size bytes in size?
			const auto file_size = size_t(file_.stats().st_size);
			if(file_size % sector_size) throw std::exception();
		}

		/* MassStorageDevices overrides. */
		size_t get_block_size() final {
			return sector_size;
		}

		size_t get_number_of_blocks() final {
			return size_t(file_.stats().st_size) / sector_size;
		}

		std::vector<uint8_t> get_block(size_t address) final {
			file_.seek(long(address * sector_size), SEEK_SET);
			return file_.read(sector_size);
		}

		void set_block(size_t address, const std::vector<uint8_t> &contents) final {
			assert(contents.size() == sector_size);
			file_.seek(long(address * sector_size), SEEK_SET);
			file_.write(contents);
		}

	private:
		FileHolder file_;
};

}
}

#endif /* RawSectorDump_h */
