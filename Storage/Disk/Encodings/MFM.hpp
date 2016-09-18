//
//  MFM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Encodings_MFM_hpp
#define Storage_Disk_Encodings_MFM_hpp

#include <cstdint>

namespace Storage {
namespace Encodings {

class Shifter {
	public:
		virtual void shift(uint8_t input) = 0;
		void add_sync();
		uint16_t output;
};

class MFMShifter: public Shifter {
	public:
		void shift(uint8_t input);
};

class FMShifter: public Shifter {
	public:
		void shift(uint8_t input);
};

}
}

#endif /* MFM_hpp */
