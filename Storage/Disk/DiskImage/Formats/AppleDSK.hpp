//
//  AppleDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef AppleDSK_hpp
#define AppleDSK_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing an Apple DSK disk image: a representation of sector contents,
	implicitly numbered and located.
*/
class AppleDSK: public DiskImage {
	public:
		/*!
			Construct an @c AppleDSK containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain a .G64 format image.
		*/
		AppleDSK(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() override;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) override;

		// TEST!
		bool get_is_read_only() override { return false; }

	private:
		Storage::FileHolder file_;
		int sectors_per_track_ = 16;
		bool is_prodos_ = false;
};

}
}


#endif /* AppleDSK_hpp */
