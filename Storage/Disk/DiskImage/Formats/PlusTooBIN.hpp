//
//  PlusTooBIN.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef PlusTooBIN_hpp
#define PlusTooBIN_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage capturing the raw bitstream contained in a PlusToo-style BIN file.
*/
class PlusTooBIN: public DiskImage {
	public:
		PlusTooBIN(const std::string &file_name);

		// Implemented to satisfy @c DiskImage.
		HeadPosition get_maximum_head_position() override;
		int get_head_count() override;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) override;

	private:
		Storage::FileHolder file_;
};

}
}


#endif /* PlusTooBIN_hpp */
