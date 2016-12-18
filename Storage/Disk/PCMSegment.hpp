//
//  PCMSegment.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMSegment_hpp
#define PCMSegment_hpp

#include <cstdint>
#include <vector>

#include "../Storage.hpp"
#include "Disk.hpp"

namespace Storage {
namespace Disk {

/*!
	A segment of PCM-sampled data.

	Bits from each byte are taken MSB to LSB.
*/
struct PCMSegment {
	Time length_of_a_bit;
	unsigned int number_of_bits;
	std::vector<uint8_t> data;
};

class PCMSegmentEventSource {
	public:
		PCMSegmentEventSource(const PCMSegment &segment);

		Track::Event get_next_event();
		void reset();

	private:
		PCMSegment segment_;
		size_t bit_pointer_;
		Track::Event next_event_;
};

}
}

#endif /* PCMSegment_hpp */
