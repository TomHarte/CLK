//
//  HFE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef HFE_hpp
#define HFE_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an HFE disk image — a bit stream representation of a floppy.
*/
class HFE: public DiskImage, public Storage::FileHolder {
	public:
		/*!
			Construct an @c SSD containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotSSD if the file doesn't appear to contain a .SSD format image.
		*/
		HFE(const char *file_name);
		~HFE();

		enum {
			ErrorNotHFE,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		unsigned int get_head_count();
		using Storage::FileHolder::get_is_read_only;
		void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track);
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		uint16_t seek_track(unsigned int head, unsigned int position);

		unsigned int head_count_;
		unsigned int track_count_;
		long track_list_offset_;
};

}
}


#endif /* HFE_hpp */
