//
//  NIB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef NIB_hpp
#define NIB_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage describing an Apple NIB disk image:
	mostly a bit stream capture, but syncs are implicitly packed
	into 8 bits instead of 9.
*/
class NIB: public DiskImage {
	public:
		NIB(const std::string &file_name);

		HeadPosition get_maximum_head_position() override;

		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) override;

	private:
		FileHolder file_;
		long get_file_offset_for_position(Track::Address address);

};

}
}

#endif /* NIB_hpp */
