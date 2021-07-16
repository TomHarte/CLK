//
//  AmigaADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef AmigaADF_hpp
#define AmigaADF_hpp

#include "MFMSectorDump.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing an Amiga ADF, which is an MFM sector contents dump,
	but the Amiga doesn't use IBM-style sector demarcation.
*/
class AmigaADF: public DiskImage {
	public:
		/*!
			Construct an @c AmigaADF containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain an .ADF format image.
		*/
		AmigaADF(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	private:
		Storage::FileHolder file_;
		long get_file_offset_for_position(Track::Address address);

};

}
}

#endif /* AmigaADF_hpp */
