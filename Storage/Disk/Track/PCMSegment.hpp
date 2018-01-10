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
#include <memory>
#include <vector>

#include "../../Storage.hpp"
#include "Track.hpp"

namespace Storage {
namespace Disk {

/*!
	A segment of PCM-sampled data.

	Bits from each byte are taken MSB to LSB.
*/
struct PCMSegment {
	Time length_of_a_bit;
	unsigned int number_of_bits = 0;
	std::vector<uint8_t> data;

	PCMSegment(Time length_of_a_bit, unsigned int number_of_bits, std::vector<uint8_t> data)
		: length_of_a_bit(length_of_a_bit), number_of_bits(number_of_bits), data(data) {}
	PCMSegment() {}

	int bit(std::size_t index) const {
		return (data[index >> 3] >> (7 ^ (index & 7)))&1;
	}

	void clear() {
		number_of_bits = 0;
		data.clear();
	}
};

/*!
	Provides a stream of events by inspecting a PCMSegment.
*/
class PCMSegmentEventSource {
	public:
		/*!
			Constructs a @c PCMSegmentEventSource that will derive events from @c segment.
			The event source is initially @c reset.
		*/
		PCMSegmentEventSource(const PCMSegment &);

		/*!
			Copy constructor; produces a segment event source with the same underlying segment
			but a unique pointer into it.
		*/
		PCMSegmentEventSource(const PCMSegmentEventSource &);

		/*!
			@returns the next event that will occur in this event stream.
		*/
		Track::Event get_next_event();

		/*!
			Resets the event source to the beginning of its event stream, exactly as if
			it has just been constructed.
		*/
		void reset();

		/*!
			Seeks as close to @c time_from_start as the event source can manage while not
			exceeding it.

			@returns the time the source is now at.
		*/
		Time seek_to(const Time &time_from_start);

		/*!
			@returns the total length of the stream of data that the source will provide.
		*/
		Time get_length();

	private:
		std::shared_ptr<PCMSegment> segment_;
		std::size_t bit_pointer_;
		Track::Event next_event_;
};

}
}

#endif /* PCMSegment_hpp */
