//
//  Shifter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Shifter_hpp
#define Shifter_hpp

#include <cstdint>
#include <memory>
#include "../../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

class Shifter {
	public:
		Shifter();
		Shifter(NumberTheory::CRC16 *crc_generator);

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
		NumberTheory::CRC16 &get_crc_generator() {
			return *crc_generator_;
		}

	private:
		// Bit stream input state
		int bits_since_token_ = 0;
		int shift_register_ = 0;
		bool is_awaiting_marker_value_ = false;
		bool should_obey_syncs_;
		Token token_;

		// input configuration
		bool is_double_density_ = false;

		std::unique_ptr<NumberTheory::CRC16> owned_crc_generator_;
		NumberTheory::CRC16 *crc_generator_;
};

}
}
}

#endif /* Shifter_hpp */
