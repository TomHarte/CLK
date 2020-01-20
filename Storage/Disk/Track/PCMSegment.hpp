//
//  PCMSegment.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMSegment_hpp
#define PCMSegment_hpp

#include <cstdint>
#include <memory>
#include <vector>

#include "../../Storage.hpp"
#include "../../../Numeric/LFSR.hpp"
#include "Track.hpp"

namespace Storage {
namespace Disk {

/*!
	A segment of PCM-sampled data.
*/
struct PCMSegment {
	/*!
		Determines the amount of space that each bit of data occupies;
		allows PCMSegments of different densities.
	*/
	Time length_of_a_bit = Time(1);

	/*!
		This is the actual data, taking advantage of the std::vector<bool>
		specialisation to use whatever one-bit-per-value encoding is
		most suited to this architecture.

		If a value is @c true then a flux transition occurs in that window.
		If it is @c false then no flux transition occurs.
	*/
	std::vector<bool> data;

	/*!
		If a segment has a fuzzy mask then anywhere the mask has a value
		of @c true, a random bit will be ORd onto whatever is in the
		corresponding slot in @c data.
	*/
	std::vector<bool> fuzzy_mask;

	/*!
		Constructs an instance of PCMSegment with the specified @c length_of_a_bit
		and @c data.
	*/
	PCMSegment(Time length_of_a_bit, const std::vector<bool> &data)
		: length_of_a_bit(length_of_a_bit), data(data) {}

	/*!
		Constructs an instance of PCMSegment where each bit window is 1 unit of time
		long and @c data is populated from the supplied @c source by serialising it
		from MSB to LSB for @c number_of_bits.
	*/
	PCMSegment(size_t number_of_bits, const uint8_t *source)
		: data(number_of_bits, false) {
		for(size_t c = 0; c < number_of_bits; ++c) {
			if((source[c >> 3] >> (7 ^ (c & 7)))&1) {
				data[c] = true;
			}
		}
	}

	/*!
		Constructs an instance of PCMSegment where each bit window is the length
		specified by @c length_of_a_bit, and @c data is populated from the supplied
		@c source by serialising it from MSB to LSB for @c number_of_bits.
	*/
	PCMSegment(Time length_of_a_bit, size_t number_of_bits, const uint8_t *source)
		: PCMSegment(number_of_bits, source) {
		this->length_of_a_bit = length_of_a_bit;
	}

	/*!
		Constructs an instance of PCMSegment where each bit window is the length
		specified by @c length_of_a_bit, and @c data is populated from the supplied
		@c source by serialising it from MSB to LSB for @c number_of_bits.
	*/
	PCMSegment(Time length_of_a_bit, size_t number_of_bits, const std::vector<uint8_t> &source) :
		PCMSegment(length_of_a_bit, number_of_bits, source.data()) {}

	/*!
		Constructs an instance of PCMSegment where each bit window is 1 unit of time
		long and @c data is populated from the supplied @c source by serialising it
		from MSB to LSB for @c number_of_bits.
	*/
	PCMSegment(size_t number_of_bits, const std::vector<uint8_t> &source) :
		PCMSegment(number_of_bits, source.data()) {}

	/*!
		Constructs an instance of PCMSegment where each bit window is 1 unit of time
		long and @c data is populated from the supplied @c source by serialising it
		from MSB to LSB, assuming every bit provided is used.
	*/
	PCMSegment(const std::vector<uint8_t> &source) :
		PCMSegment(source.size() * 8, source.data()) {}

	/*!
		Constructs an instance of PCMSegment where each bit window is 1 unit of time
		long and @c data is empty.
	*/
	PCMSegment() {}

	/// Empties the PCMSegment.
	void clear() {
		data.clear();
	}

	/*!
		Rotates all bits in this segment by @c length bits.

		@c length is signed; to rotate left provide a negative number.
	*/
	void rotate_right(size_t length);

	/*!
		Produces a byte buffer where the contents of @c data are serialised into bytes

		If @c msb_first is @c true then each byte is expected to be deserialised from
		MSB to LSB.

		If @c msb_first is @c false then each byte is expected to be deserialised from
		LSB to MSB.
	*/
	std::vector<uint8_t> byte_data(bool msb_first = true) const {
		std::vector<uint8_t> bytes((data.size() + 7) >> 3);
		size_t pointer = 0;
		const size_t pointer_mask = msb_first ? 7 : 0;
		for(const auto bit: data) {
			if(bit) bytes[pointer >> 3] |= 1 << ((pointer & 7) ^ pointer_mask);
			++pointer;
		}
		return bytes;
	}

	/// Appends the data of @c rhs to the current data. Does not adjust @c length_of_a_bit.
	PCMSegment &operator +=(const PCMSegment &rhs);

	/// @returns the total amount of time occupied by all the data stored in this segment.
	Time length() const {
		return length_of_a_bit * static_cast<unsigned int>(data.size());
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

		/*!
			@returns a reference to the underlying segment.
		*/
		const PCMSegment &segment() const;
		PCMSegment &segment();

	private:
		std::shared_ptr<PCMSegment> segment_;
		std::size_t bit_pointer_;
		Track::Event next_event_;
		Numeric::LFSR<uint64_t> lfsr_;
};

}
}

#endif /* PCMSegment_hpp */
