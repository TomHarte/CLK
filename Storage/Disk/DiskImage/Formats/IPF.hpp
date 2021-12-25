//
//  IPF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef IPF_hpp
#define IPF_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"
#include "../../../TargetPlatforms.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing an IPF.
*/
class IPF: public DiskImage, public TargetPlatform::TypeDistinguisher {
	public:
		/*!
			Construct an @c IPF containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain an .HFE format image.
			@throws Error::UnknownVersion if the file looks correct but is an unsupported version.
		*/
		IPF(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	private:
		Storage::FileHolder file_;
		uint16_t seek_track(Track::Address address);

		int head_count_;
		int track_count_;

		TargetPlatform::Type target_platform_type() final {
			return platform_type_;
		}
		TargetPlatform::Type platform_type_ = TargetPlatform::Amiga;
};

}
}

#endif /* IPF_hpp */
