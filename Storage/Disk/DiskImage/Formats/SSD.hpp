//
//  SSD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef SSD_hpp
#define SSD_hpp

#include "MFMSectorDump.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing a DSD or SSD disk image: a decoded sector dump of an Acorn DFS disk.
*/
class SSD: public MFMSectorDump {
	public:
		/*!
			Construct an @c SSD containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain a .SSD format image.
		*/
		SSD(const std::string &file_name);

		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;

	private:
		long get_file_offset_for_position(Track::Address address) final;

		int head_count_;
		int track_count_;
};

}
}

#endif /* SSD_hpp */
