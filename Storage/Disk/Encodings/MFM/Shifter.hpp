//
//  Shifter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Shifter_hpp
#define Shifter_hpp

#include <cstdint>
#include <memory>
#include "../../../../Numeric/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

/*!
	The MFM shifter parses a stream of bits as input in order to produce
	a stream of MFM tokens as output. So e.g. it is suitable for use in parsing
	the output of a PLL windowing of disk events.

	It supports both FM and MFM parsing; see @c set_is_double_density.

	It will ordinarily honour sync patterns; that should be turned off when within
	a sector because false syncs can occur. See @c set_should_obey_syncs.

	It aims to implement the same behaviour as WD177x-series controllers when
	detecting a false sync — the received byte value will be either a 0xc1 or 0x14,
	depending on phase.

	Bits should be fed in with @c add_input_bit.

	The current output token can be read with @c get_token. It will usually be None but
	may indicate that an index, ID, data or deleted data mark was found, that an
	MFM sync mark was found, or that an ordinary byte has been decoded.

	It will properly reset and/or seed a CRC generator based on the data and ID marks,
	and feed it with incoming bytes. You can access that CRC generator to query its
	value via @c get_crc_generator(). An easy way to check whether the disk contained
	a proper CRC is to read bytes until you've just read whatever CRC was on the disk,
	then check that the generator has a value of zero.

	A specific instance of the CRC generator can be supplied at construction if preferred.
*/
class Shifter {
	public:
		Shifter();
		Shifter(CRC::CCITT *crc_generator);

		void set_is_double_density(bool is_double_density);
		void set_should_obey_syncs(bool should_obey_syncs);
		void add_input_bit(int bit);

		enum Token {
			Index, ID, Data, DeletedData, Sync, Byte, None
		};
		uint8_t get_byte() const;
		Token get_token() const {
			return token_;
		}
		CRC::CCITT &get_crc_generator() {
			return *crc_generator_;
		}

	private:
		// Bit stream input state.
		int bits_since_token_ = 0;
		unsigned int shift_register_ = 0;
		bool is_awaiting_marker_value_ = false;
		bool should_obey_syncs_ = true;
		Token token_ = None;

		// Input configuration.
		bool is_double_density_ = false;

		std::unique_ptr<CRC::CCITT> owned_crc_generator_;
		CRC::CCITT *crc_generator_;
};

}
}
}

#endif /* Shifter_hpp */
