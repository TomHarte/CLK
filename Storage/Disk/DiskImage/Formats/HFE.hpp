//
//  HFE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef HFE_hpp
#define HFE_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a @c Disk containing an HFE disk image — a bit stream representation of a floppy.
*/
class HFE: public DiskImage {
	public:
		/*!
			Construct an @c SSD containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotSSD if the file doesn't appear to contain a .SSD format image.
		*/
		HFE(const char *file_name);
		~HFE();

		enum {
			ErrorNotHFE,
		};

		// implemented to satisfy @c Disk
		int get_head_position_count() override;
		int get_head_count() override;
		bool get_is_read_only() override;
		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) override;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) override;

	private:
		Storage::FileHolder file_;
		uint16_t seek_track(Track::Address address);

		int head_count_;
		int track_count_;
		long track_list_offset_;
};

}
}


#endif /* HFE_hpp */
