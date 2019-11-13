//
//  ST.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef ST_hpp
#define ST_hpp

#include "MFMSectorDump.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing an ST disk image: a decoded sector dump of an Atari ST disk.
*/
class ST: public MFMSectorDump {
	public:
		/*!
			Construct an @c ST containing content from the file with name @c file_name.

			@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
			@throws Error::InvalidFormat if the file doesn't appear to contain a .ST format image.
		*/
		ST(const std::string &file_name);

		HeadPosition get_maximum_head_position() override;
		int get_head_count() override;

	private:
		long get_file_offset_for_position(Track::Address address) override;

		int head_count_;
		int track_count_;
};

}
}

#endif /* ST_hpp */
