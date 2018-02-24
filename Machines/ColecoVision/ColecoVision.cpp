//
//  ColecoVision.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ColecoVision.hpp"

namespace Coleco {
namespace Vision {

class ConcreteMachine:
	public Machine {
};

}
}

using namespace Coleco::Vision;

Machine *Machine::ColecoVision() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
