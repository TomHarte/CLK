//
//  MultiKeyboardMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiKeyboardMachine_hpp
#define MultiKeyboardMachine_hpp

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Machines/KeyboardMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides a class that multiplexes the keyboard machine interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
class MultiKeyboardMachine: public KeyboardMachine::Machine {
	public:
		MultiKeyboardMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		// Below is the standard KeyboardMachine::Machine interface; see there for documentation.
		void clear_all_keys() override;
		void set_key_state(uint16_t key, bool is_pressed) override;
		void type_string(const std::string &) override;
		Inputs::Keyboard &get_keyboard() override;

	private:
		std::vector<::KeyboardMachine::Machine *> machines_;
		Inputs::Keyboard keyboard_;
};

}
}

#endif /* MultiKeyboardMachine_hpp */
