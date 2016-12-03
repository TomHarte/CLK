//
//  Drive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Drive_hpp
#define Drive_hpp

#include <memory>
#include "Disk.hpp"

namespace Storage {
namespace Disk {

class Drive {
	public:
		Drive();

		/*!
			Inserts @c disk into the drive.
		*/
		void set_disk(std::shared_ptr<Disk> disk);

		/*!
			@returns @c true if a disk is currently inserted; @c false otherwise.
		*/
		bool has_disk();

		/*!
			@returns @c true if the drive head is currently at track zero; @c false otherwise.
		*/
		bool get_is_track_zero();

		/*!
			Steps the disk head the specified number of tracks. Positive numbers step inwards, negative numbers
			step outwards.
		*/
		void step(int direction);

		/*!
		*/
		void set_head(unsigned int head);

		std::shared_ptr<Track> get_track();

	private:
		std::shared_ptr<Disk> disk_;
		int head_position_;
		unsigned int head_;
};


}
}

#endif /* Drive_hpp */
