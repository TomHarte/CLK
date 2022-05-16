//
//  68000Mk2Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef _8000Mk2Storage_h
#define _8000Mk2Storage_h

namespace CPU {
namespace MC68000Mk2 {

struct ProcessorBase {
	enum State: int {
		Reset = -1,
	};

	HalfCycles time_remaining;
	int state = State::Reset;
};

}
}

#endif /* _8000Mk2Storage_h */
