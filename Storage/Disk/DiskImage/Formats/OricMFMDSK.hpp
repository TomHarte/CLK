//
//  OricMFMDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef OricMFMDSK_hpp
#define OricMFMDSK_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing an Oric MFM-stype disk image: a stream of the MFM data bits with clocks omitted.
*/
class OricMFMDSK: public DiskImage {
	public:
		/*!
			Construct an @c OricMFMDSK containing content from the file with name @c file_name.

			@throws ErrorNotOricMFMDSK if the file doesn't appear to contain an Oric MFM format image.
		*/
		OricMFMDSK(const std::string &file_name);

		// implemented to satisfy @c DiskImage
		HeadPosition get_maximum_head_position() final;
		int get_head_count() final;
		bool get_is_read_only() final;

		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	private:
		Storage::FileHolder file_;
		long get_file_offset_for_position(Track::Address address);

		uint32_t head_count_;
		uint32_t track_count_;
		uint32_t geometry_type_;
};

}
}

#endif /* OricMFMDSK_hpp */
