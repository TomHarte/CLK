//
//  MultiMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiMachine_hpp
#define MultiMachine_hpp

#include "../../../Machines/DynamicMachine.hpp"

#include "Implementation/MultiConfigurable.hpp"
#include "Implementation/MultiCRTMachine.hpp"
#include "Implementation/MultiJoystickMachine.hpp"
#include "Implementation/MultiKeyboardMachine.hpp"
#include "Implementation/MultiMediaTarget.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides the same interface as to a single machine, while multiplexing all
	underlying calls to an array of real dynamic machines.

	Calls to crt_machine->get_crt will return that for the frontmost machine;
	anything installed as the speaker's delegate will similarly receive
	feedback only from that machine.

	Following each crt_machine->run_for, reorders the supplied machines by
	confidence.

	If confidence for any machine becomes disproportionately low compared to
	the others in the set, that machine stops running.
*/
class MultiMachine: public ::Machine::DynamicMachine, public MultiCRTMachine::Delegate {
	public:
		/*!
			Allows a potential MultiMachine creator to enquire as to whether there's any benefit in
			requesting this class as a proxy.

			@returns @c true if the multimachine would discard all but the first machine in this list;
				@c false otherwise.
		*/
		static bool would_collapse(const std::vector<std::unique_ptr<DynamicMachine>> &machines);
		MultiMachine(std::vector<std::unique_ptr<DynamicMachine>> &&machines);

		Activity::Source *activity_source() final;
		Configurable::Device *configurable_device() final;
		CRTMachine::Machine *crt_machine() final;
		JoystickMachine::Machine *joystick_machine() final;
		MouseMachine::Machine *mouse_machine() final;
		KeyboardMachine::Machine *keyboard_machine() final;
		MediaTarget::Machine *media_target() final;
		void *raw_pointer() final;

	private:
		void multi_crt_did_run_machines() final;

		std::vector<std::unique_ptr<DynamicMachine>> machines_;
		std::recursive_mutex machines_mutex_;

		MultiConfigurable configurable_;
		MultiCRTMachine crt_machine_;
		MultiJoystickMachine joystick_machine_;
		MultiKeyboardMachine keyboard_machine_;
		MultiMediaTarget media_target_;

		void pick_first();
		bool has_picked_ = false;
};

}
}

#endif /* MultiMachine_hpp */
