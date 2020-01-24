//
//  NIB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef NIB_hpp
#define NIB_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage describing an Apple NIB disk image:
	a bit stream capture that omits sync zeroes, and doesn't define
	the means for full reconstruction.
*/
class NIB: public DiskImage {
	public:
		NIB(const std::string &file_name);

		// Implemented to satisfy @c DiskImage.
		HeadPosition get_maximum_head_position() final;
		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) final;
		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) final;
		bool get_is_read_only() final;

	private:
		FileHolder file_;
		long get_file_offset_for_position(Track::Address address);
		long file_offset(Track::Address address);
};

}
}

#endif /* NIB_hpp */
