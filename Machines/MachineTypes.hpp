//
//  MachineTypes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef MachineTypes_h
#define MachineTypes_h

// Rounds up everything in the MachineTypes namespace, all being
// optional (at least, semantically) interfaces that machines
// might implement. These header files are intended to be light,
// so including all shouldn't be a huge burden.

#include "AudioProducer.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "MediaTarget.hpp"
#include "MouseMachine.hpp"
#include "ScanProducer.hpp"
#include "StateProducer.hpp"
#include "TimedMachine.hpp"

#endif /* MachineTypes_h */
