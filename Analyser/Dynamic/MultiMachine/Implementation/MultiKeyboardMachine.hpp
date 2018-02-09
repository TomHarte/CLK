//
//  MultiKeyboardMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiKeyboardMachine_hpp
#define MultiKeyboardMachine_hpp

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Machines/KeyboardMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

class MultiKeyboardMachine: public ::KeyboardMachine::Machine {
	public:
		MultiKeyboardMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		void clear_all_keys() override;
		void set_key_state(uint16_t key, bool is_pressed) override;
		void type_string(const std::string &) override;
		void keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) override;

	private:
		std::vector<::KeyboardMachine::Machine *> machines_;
};

}
}

#endif /* MultiKeyboardMachine_hpp */
