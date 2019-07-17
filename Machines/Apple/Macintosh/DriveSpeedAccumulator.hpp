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

	private:
		std::array<uint8_t, 512> samples_;
		std::size_t sample_pointer_ = 0;
};

}
}

#endif /* DriveSpeedAccumulator_hpp */
