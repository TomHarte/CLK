//
//  MemorySwitches.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/06/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef MemorySwitches_h
#define MemorySwitches_h

namespace Apple {
namespace II {

enum PagingType: int {
	Main = 1 << 0,
	ZeroPage = 1 << 1,
	CardArea = 1 << 2,
	LanguageCard = 1 << 3,
};

}
}

#endif /* MemorySwitches_h */
