//
//  z8530.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef z8530_hpp
#define z8530_hpp

#include <cstdint>

namespace Zilog {
namespace SCC {

/*!
	Models the Zilog 8530 SCC, a serial adaptor.
*/
class z8530 {
	public:
		void reset();

		/*
			Notes on addressing below:

			There's no inherent ordering of the two 'address' lines,
			A/B and C/D, but the methods below assume:

				A/B = A0
				C/D = A1
		*/

		std::uint8_t read(int address);
		void write(int address, std::uint8_t value);

	private:
		class Channel {
			public:
				uint8_t read(bool data);
				void write(bool data, uint8_t pointer, uint8_t value);

			private:
				uint8_t data_ = 0xff;
		} channels_[2];
		uint8_t pointer_ = 0;
};

}
}


#endif /* z8530_hpp */
