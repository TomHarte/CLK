//
//  CPCDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CPCDSK_hpp
#define CPCDSK_hpp

#include "../Disk.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an Amstrad CPC-stype disk image — some arrangement of sectors with status bits.
*/
class CPCDSK: public Disk, public Storage::FileHolder {
	public:
		/*!
			Construct an @c AcornADF containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotAcornADF if the file doesn't appear to contain an Acorn .ADF format image.
		*/
		CPCDSK(const char *file_name);

		enum {
			ErrorNotCPCDSK,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		unsigned int get_head_count();
		bool get_is_read_only();

	private:
		std::shared_ptr<Track> get_uncached_track_at_position(unsigned int head, unsigned int position);

		unsigned int head_count_;
		unsigned int head_position_count_;
		bool is_extended_;
};

}
}


#endif /* CPCDSK_hpp */
