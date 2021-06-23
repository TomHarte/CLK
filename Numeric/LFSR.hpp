//
//  LFSR.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef LFSR_h
#define LFSR_h

#include <cstdint>
#include <cstdlib>

#include "Sizes.hpp"

namespace Numeric {

template <typename IntType> struct LSFRPolynomial {};

// The following were taken 'at random' from https://users.ece.cmu.edu/~koopman/lfsr/index.html
template <> struct LSFRPolynomial<uint64_t> {
	static constexpr uint64_t value = 0x80000000000019E2;
};

template <> struct LSFRPolynomial<uint32_t> {
	static constexpr uint32_t value = 0x80000C34;
};

template <> struct LSFRPolynomial<uint16_t> {
	static constexpr uint16_t value = 0x853E;
};

template <> struct LSFRPolynomial<uint8_t> {
	static constexpr uint8_t value = 0xAF;
};

/*!
	Provides a linear-feedback shift register with a random initial state; if no polynomial is supplied
	then one will be picked that is guaranteed to give the maximal number of LFSR states that can fit
	in the specified int type.
*/
template <typename IntType = uint64_t, IntType polynomial = LSFRPolynomial<IntType>::value> class LFSR {
	public:
		/*!
			Constructs an LFSR with a random initial value.
		*/
		constexpr LFSR() noexcept {
			// Randomise the value, ensuring it doesn't end up being 0;
			// don't set any top bits, in case this is a signed type.
			while(!value_) {
				uint8_t *value_byte = reinterpret_cast<uint8_t *>(&value_);
				for(size_t c = 0; c < sizeof(IntType); ++c) {
					*value_byte = uint8_t(uint64_t(rand()) * 127 / RAND_MAX);
					++value_byte;
				}
			}
		}

		/*!
			Constructs an LFSR with the specified initial value.

			An initial value of 0 is invalid.
		*/
		LFSR(IntType initial_value) : value_(initial_value) {}

		/*!
			Advances the LSFR, returning either an @c IntType of value @c 1 or @c 0,
			determining the bit that was just shifted out.
		*/
		IntType next() {
			const auto result = value_ & 1;
			value_ = (value_ >> 1) ^ (result * polynomial);
			return result;
		}

	private:
		IntType value_ = 0;
};

template <uint64_t polynomial> class LFSRv: public LFSR<typename MinIntTypeValue<polynomial>::type, polynomial> {};

}

#endif /* LFSR_h */
