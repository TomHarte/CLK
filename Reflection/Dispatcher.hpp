//
//  Dispatcher.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Dispatcher_hpp
#define Dispatcher_hpp

#include <cstdint>

namespace Reflection {

/*!
	Calls @c t.dispatch<c>(args...) as efficiently as possible.
*/
template <typename TargetT, typename... Args> void dispatch(TargetT &t, uint8_t c, Args &&... args) {
#define Opt(x)		case x: t.template dispatch<x>(std::forward<Args>(args)...);	break
#define Opt4(x)		Opt(x + 0x00);		Opt(x + 0x01);		Opt(x + 0x02);		Opt(x + 0x03)
#define Opt16(x)	Opt4(x + 0x00);		Opt4(x + 0x04);		Opt4(x + 0x08);		Opt4(x + 0x0c)
#define Opt64(x)	Opt16(x + 0x00);	Opt16(x + 0x10);	Opt16(x + 0x20);	Opt16(x + 0x30)
#define Opt256(x)	Opt64(x + 0x00);	Opt64(x + 0x40);	Opt64(x + 0x80);	Opt64(x + 0xc0)

	switch(c) {
		Opt256(0);
	}

#undef Opt256
#undef Opt64
#undef Opt16
#undef Opt4
#undef Opt
}

}

#endif /* Dispatcher_hpp */
