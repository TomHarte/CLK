//
//  STX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef STX_hpp
#define STX_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing an STX disk image: sector contents plus a bunch of annotations as to sizing,
	placement, bit density, fuzzy bits, etc.
*/
class STX: public DiskImage {
	public:
		/*!
			Construct an @c STX containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain a .STX format image.
		*/
		STX(const std::string &file_name);

		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;

		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) final;

	private:
		FileHolder file_;

		int track_count_;
		bool is_new_format_;
		long offset_by_track_[256];
};

}
}

#endif /* STX_hpp */
