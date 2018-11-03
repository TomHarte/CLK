//
//  CRTTypes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTTypes_h
#define CRTTypes_h

namespace Outputs {
namespace CRT {

enum class DisplayType {
	PAL50,
	NTSC60
};

enum class VideoSignal {
	RGB,
	SVideo,
	Composite
};

}
}

#endif /* CRTTypes_h */
