//
//  StandardOptions.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef StandardOptions_hpp
#define StandardOptions_hpp

#include "../Reflection/Enum.h"

namespace Configurable {

ReflectableEnum(Display,
	RGB,
	SVideo,
	CompositeColour,
	CompositeMonochrome
);


}

#endif /* StandardOptions_hpp */
