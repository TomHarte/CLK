//
//  SSD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef SSD_hpp
#define SSD_hpp

#include "MFMSectorDump.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing a DSD or SSD disk image — a decoded sector dump of an Acorn DFS disk.
*/
class SSD: public MFMSectorDump {
	public:
		/*!
			Construct an @c SSD containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotSSD if the file doesn't appear to contain a .SSD format image.
		*/
		SSD(const char *file_name);

		enum {
			ErrorNotSSD,
		};

		int get_head_position_count() override;
		int get_head_count() override;

	private:
		long get_file_offset_for_position(Track::Address address) override;

		int head_count_;
		int track_count_;
};

}
}

#endif /* SSD_hpp */
