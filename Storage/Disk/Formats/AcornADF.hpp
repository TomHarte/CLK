//
//  AcornADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef AcornADF_hpp
#define AcornADF_hpp

#include "../DiskImage.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an ADF disk image — a decoded sector dump of an Acorn ADFS disk.
*/
class AcornADF: public DiskImage, public Storage::FileHolder {
	public:
		/*!
			Construct an @c AcornADF containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotAcornADF if the file doesn't appear to contain an Acorn .ADF format image.
		*/
		AcornADF(const char *file_name);

		enum {
			ErrorNotAcornADF,
		};

		unsigned int get_head_position_count();
		unsigned int get_head_count();
		bool get_is_read_only();
		void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track);
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		std::mutex file_access_mutex_;
		long get_file_offset_for_position(unsigned int head, unsigned int position);
};

}
}

#endif /* AcornADF_hpp */
