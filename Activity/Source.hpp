//
//  ActivitySource.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Observer.hpp"

namespace Activity {

class Source {
	public:
		virtual void set_activity_observer(Observer *observer) = 0;
};

}
