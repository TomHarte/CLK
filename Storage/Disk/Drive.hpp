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
			Replaces whatever is in the drive with @c disk.
		*/
		void set_disk(const std::shared_ptr<Disk> &disk);

		/*!
			Replaces whatever is in the drive with a disk that contains endless copies of @c track.
		*/
		void set_disk_with_track(const std::shared_ptr<Track> &track);

		/*!
			@returns @c true if a disk is currently inserted; @c false otherwise.
		*/
		bool has_disk();

		/*!
			@returns @c true if the drive head is currently at track zero; @c false otherwise.
		*/
		bool get_is_track_zero();

		/*!
			Steps the disk head the specified number of tracks. Positive numbers step inwards (i.e. away from track 0),
			negative numbers step outwards (i.e. towards track 0).
		*/
		void step(int direction);

		/*!
			Sets the current read head.
		*/
		void set_head(unsigned int head);

		/*!
			@returns @c true if the inserted disk is read-only; @c false otherwise.
		*/
		bool get_is_read_only();

		/*!
			@returns the track underneath the current head at the location now stepped to.
		*/
		std::shared_ptr<Track> get_track();

		/*!
			Attempts to set @c track as the track underneath the current head at the location now stepped to.
		*/
		void set_track(const std::shared_ptr<Track> &track);

	private:
		std::shared_ptr<Track> track_;
		std::shared_ptr<Disk> disk_;
		int head_position_;
		unsigned int head_;
};


}
}

#endif /* Drive_hpp */
