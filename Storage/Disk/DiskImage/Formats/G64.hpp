//
//  G64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef G64_hpp
#define G64_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing a G64 disk image — a raw but perfectly-clocked GCR stream.
*/
class G64: public DiskImage {
	public:
		/*!
			Construct a @c G64 containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotG64 if the file doesn't appear to contain a .G64 format image.
			@throws ErrorUnknownVersion if this file appears to be a .G64 but has an unrecognised version number.
		*/
		G64(const char *file_name);

		enum {
			ErrorCantOpen,
			ErrorNotG64,
			ErrorUnknownVersion
		};

		// implemented to satisfy @c Disk
		int get_head_position_count() override;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) override;
		using DiskImage::get_is_read_only;

	private:
		Storage::FileHolder file_;
		uint8_t number_of_tracks_;
		uint16_t maximum_track_size_;
};

}
}

#endif /* G64_hpp */
