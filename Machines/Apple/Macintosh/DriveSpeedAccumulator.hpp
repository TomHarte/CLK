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

namespace Apple {
namespace Macintosh {

class DriveSpeedAccumulator {
	public:
		/*!
			Accepts fetched motor control values.
		*/
		void post_sample(uint8_t sample);

		struct Delegate {
			virtual void drive_speed_accumulator_set_drive_speed(DriveSpeedAccumulator *, float speed) = 0;
		};
		/*!
			Sets the delegate to receive drive speed changes.
		*/
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;;
		}

	private:
		static constexpr int samples_per_bucket = 20;
		int sample_count_ = 0;
		int sample_total_ = 0;
		Delegate *delegate_ = nullptr;
};

}
}

#endif /* DriveSpeedAccumulator_hpp */
