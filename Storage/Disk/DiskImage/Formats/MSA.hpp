//
//  MSA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MSA_hpp
#define MSA_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <vector>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage describing an Atari ST MSA disk image:
	a track dump with some metadata and potentially patches of RLE compression.
*/
class MSA final: public DiskImage {
	public:
		MSA(const std::string &file_name);

		// Implemented to satisfy @c DiskImage.
		HeadPosition get_maximum_head_position() override;
		int get_head_count() override;
		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) override;

	private:
		FileHolder file_;
		uint16_t sectors_per_track_;
		uint16_t sides_;
		uint16_t starting_track_;
		uint16_t ending_track_;

		std::vector<std::vector<uint8_t>> uncompressed_tracks_;
};


}
}

#endif /* MSA_hpp */
