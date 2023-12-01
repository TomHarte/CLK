//
//  PCBooter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef PCBooter_hpp
#define PCBooter_hpp

#include "MFMSectorDump.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage holding a raw IBM PC booter disk image: a sector dump of one of a few fixed sizes
	with what looks like a meaningful boot sector.
*/
class PCBooter: public MFMSectorDump {
	public:
		PCBooter(const std::string &file_name);
		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;

	private:
		long get_file_offset_for_position(Track::Address address) final;

		int head_count_;
		int track_count_;
};

}

#endif /* PCBooter_hpp */
