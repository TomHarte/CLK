//
//  SSD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef SSD_hpp
#define SSD_hpp

#include "../Disk.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing a DSD or SSD disk image — a decoded sector dump of an Acorn DFS disk.
*/
class SSD: public Disk {
	public:
		/*!
			Construct an @c SSD containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotSSD if the file doesn't appear to contain a .SSD format image.
		*/
		SSD(const char *file_name);
		~SSD();

		enum {
			ErrorCantOpen,
			ErrorNotSSD,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		unsigned int get_head_count();
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		FILE *_file;
		unsigned int _head_count;
		unsigned int _track_count;
};

}
}

#endif /* SSD_hpp */
