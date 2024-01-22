//
//  MultiKeyboardMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Machines/KeyboardMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser::Dynamic {

/*!
	Provides a class that multiplexes the keyboard machine interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
class MultiKeyboardMachine: public MachineTypes::KeyboardMachine {
	private:
		std::vector<MachineTypes::KeyboardMachine *> machines_;

		class MultiKeyboard: public Inputs::Keyboard {
			public:
				MultiKeyboard(const std::vector<MachineTypes::KeyboardMachine *> &machines);

				bool set_key_pressed(Key key, char value, bool is_pressed, bool is_repeat) final;
				void reset_all_keys() final;
				const std::set<Key> &observed_keys() const final;
				bool is_exclusive() const final;

			private:
				const std::vector<MachineTypes::KeyboardMachine *> &machines_;
				std::set<Key> observed_keys_;
				bool is_exclusive_ = false;
		};
		std::unique_ptr<MultiKeyboard> keyboard_;

	public:
		MultiKeyboardMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		// Below is the standard KeyboardMachine::Machine interface; see there for documentation.
		void clear_all_keys() final;
		void set_key_state(uint16_t key, bool is_pressed) final;
		void type_string(const std::string &) final;
		bool can_type(char c) const final;
		Inputs::Keyboard &get_keyboard() final;
};

}
