//
//  MultiJoystickMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiJoystickMachine_hpp
#define MultiJoystickMachine_hpp

#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

class MultiJoystickMachine: public JoystickMachine::Machine {
	public:
		MultiJoystickMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override;

	private:
		std::vector<JoystickMachine::Machine *> machines_;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

};

}
}

#endif /* MultiJoystickMachine_hpp */
