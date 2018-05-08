//
//  ActivitySource.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ActivitySource_h
#define ActivitySource_h

#include "../ActivityObserver/ActivityObserver.hpp"

namespace ActivitySource {

class Machine {
	public:
		virtual void set_activity_observer(ActivityObserver *receiver) = 0;
};

}


#endif /* ActivitySource_h */
