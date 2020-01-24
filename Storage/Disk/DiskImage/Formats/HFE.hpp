//
//  HFE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef HFE_hpp
#define HFE_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing an HFE: a bit stream representation of a floppy.
*/
class HFE: public DiskImage {
	public:
		/*!
			Construct an @c HFE containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain an .HFE format image.
			@throws Error::UnknownVersion if the file looks correct but is an unsupported version.
		*/
		HFE(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;
		bool get_is_read_only() final;
		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	private:
		Storage::FileHolder file_;
		uint16_t seek_track(Track::Address address);

		int head_count_;
		int track_count_;
		long track_list_offset_;
};

}
}


#endif /* HFE_hpp */
