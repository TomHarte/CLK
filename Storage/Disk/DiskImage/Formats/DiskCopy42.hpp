//
//  DiskCopy42.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DiskCopy42_hpp
#define DiskCopy42_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing a disk imaged by Apple's Disk Copy 4.2: sector contents
	(plus tag data) in either an Apple GCR or standard MFM encoding.
*/
class DiskCopy42: public DiskImage {
	public:
		/*!
			Construct a @c DiskCopy42 containing content from the file with name @c file_name.

			@throws Error::InvalidFormat if this file doesn't appear to be in Disk Copy 4.2 format.
		*/
		DiskCopy42(const std::string &file_name);

		// implemented to satisfy @c Disk
		HeadPosition get_maximum_head_position() override;
		int get_head_count() override;
		bool get_is_read_only() override;

		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) override;

	private:
		Storage::FileHolder file_;

		enum class Encoding {
			GCR400,
			GCR800,
			MFM720,
			MFM1440
		} encoding_;
		uint8_t format_;

		std::vector<uint8_t> data_;
		std::vector<uint8_t> tags_;

		uint32_t checksum(const std::vector<uint8_t> &, size_t bytes_to_skip = 0);
};

}
}

#endif /* DiskCopy42_hpp */
