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

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing a D64 disk image — a decoded sector dump of a C1540-format disk.
*/
class D64: public Disk {
	public:
		/*!
			Construct a @c D64 containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotD64 if the file doesn't appear to contain a .D64 format image.
		*/
		D64(const char *file_name);
		~D64();

		enum {
			ErrorCantOpen,
			ErrorNotD64,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		FILE *_file;
		unsigned int _number_of_tracks;
		uint16_t _disk_id;
};

}
}

#endif /* D64_hpp */
