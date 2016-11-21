//
//  OricMFMDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef OricMFMDSK_hpp
#define OricMFMDSK_hpp

#include "../Disk.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an Oric MFM-stype disk image — an MFM bit stream.
*/
class OricMFMDSK: public Disk, public Storage::FileHolder {
	public:
		/*!
			Construct an @c AcornADF containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotAcornADF if the file doesn't appear to contain an Acorn .ADF format image.
		*/
		OricMFMDSK(const char *file_name);

		enum {
			ErrorNotOricMFMDSK,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		unsigned int get_head_count();
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		uint32_t head_count_;
		uint32_t track_count_;
		uint32_t geometry_type_;
};

}
}

#endif /* OricMFMDSK_hpp */
