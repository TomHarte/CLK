//
//  6526Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Storage_h
#define _526Storage_h

namespace MOS {
namespace MOS6526 {

struct MOS6526Storage {

	struct Registers {
		uint8_t output[2] = {0, 0};
		uint8_t data_direction[2] = {0, 0};
		uint8_t interrupt_control_ = 0;
	} registers_;

};

}
}

#endif /* _526Storage_h */
