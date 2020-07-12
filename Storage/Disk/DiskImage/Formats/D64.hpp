//
//  D64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef D64_hpp
#define D64_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing a D64 disk image: a decoded sector dump of a C1540-format disk.
*/
class D64: public DiskImage {
	public:
		/*!
			Construct a @c D64 containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain a .D64 format image.
		*/
		D64(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() final;
		using DiskImage::get_is_read_only;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	private:
		Storage::FileHolder file_;
		int number_of_tracks_;
		uint16_t disk_id_;
};

}
}

#endif /* D64_hpp */
