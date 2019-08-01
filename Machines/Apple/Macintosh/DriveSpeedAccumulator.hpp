//
//  DriveSpeedAccumulator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DriveSpeedAccumulator_hpp
#define DriveSpeedAccumulator_hpp

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../Components/DiskII/MacintoshDoubleDensityDrive.hpp"

namespace Apple {
namespace Macintosh {

class DriveSpeedAccumulator {
	public:
		/*!
			Accepts fetched motor control values.
		*/
		void post_sample(uint8_t sample);

		/*!
			Adds a connected drive. Up to two of these
			can be supplied. Only Macintosh DoubleDensityDrives
			are supported.
		*/
		void add_drive(Apple::Macintosh::DoubleDensityDrive *drive);

	private:
		std::array<uint8_t, 20> samples_;
		std::size_t sample_pointer_ = 0;
		Apple::Macintosh::DoubleDensityDrive *drives_[2] = {nullptr, nullptr};
		int number_of_drives_ = 0;
};

}
}

#endif /* DriveSpeedAccumulator_hpp */
