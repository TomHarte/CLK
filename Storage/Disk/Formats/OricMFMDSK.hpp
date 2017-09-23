//
//  OricMFMDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef OricMFMDSK_hpp
#define OricMFMDSK_hpp

#include "../DiskImage.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an Oric MFM-stype disk image — a stream of the MFM data bits with clocks omitted.
*/
class OricMFMDSK: public DiskImage, public Storage::FileHolder {
	public:
		/*!
			Construct an @c OricMFMDSK containing content from the file with name @c file_name.

			@throws ErrorNotOricMFMDSK if the file doesn't appear to contain an Oric MFM format image.
		*/
		OricMFMDSK(const char *file_name);

		enum {
			ErrorNotOricMFMDSK,
		};

		// implemented to satisfy @c Disk
		unsigned int get_head_position_count();
		unsigned int get_head_count();
		bool get_is_read_only();
		void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track);
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	private:
		std::mutex file_access_mutex_;
		long get_file_offset_for_position(unsigned int head, unsigned int position);

		uint32_t head_count_;
		uint32_t track_count_;
		uint32_t geometry_type_;
};

}
}

#endif /* OricMFMDSK_hpp */
