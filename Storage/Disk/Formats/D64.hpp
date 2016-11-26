//
//  D64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef D64_hpp
#define D64_hpp

#include "../Disk.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing a D64 disk image — a decoded sector dump of a C1540-format disk.
*/
class D64: public Disk, public Storage::FileHolder {
	public:
		/*!
			Construct a @c D64 containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotD64 if the file doesn't appear to contain a .D64 format image.
		*/
		D64(const char *file_name);

		enum {
			ErrorNotD64,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();

	private:
		std::shared_ptr<Track> virtual_get_track_at_position(unsigned int head, unsigned int position);
		unsigned int number_of_tracks_;
		uint16_t disk_id_;
};

}
}

#endif /* D64_hpp */
