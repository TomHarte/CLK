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

#include "Implementation/MultiProducer.hpp"
#include "Implementation/MultiConfigurable.hpp"
#include "Implementation/MultiProducer.hpp"
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
class MultiMachine: public ::Machine::DynamicMachine, public MultiTimedMachine::Delegate {
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
		MachineTypes::TimedMachine *timed_machine() final;
		MachineTypes::ScanProducer *scan_producer() final;
		MachineTypes::AudioProducer *audio_producer() final;
		MachineTypes::JoystickMachine *joystick_machine() final;
		MachineTypes::KeyboardMachine *keyboard_machine() final;
		MachineTypes::MouseMachine *mouse_machine() final;
		MachineTypes::MediaTarget *media_target() final;
		void *raw_pointer() final;

	private:
		void did_run_machines(MultiTimedMachine *) final;

		std::vector<std::unique_ptr<DynamicMachine>> machines_;
		std::recursive_mutex machines_mutex_;

		MultiConfigurable configurable_;
		MultiTimedMachine timed_machine_;
		MultiScanProducer scan_producer_;
		MultiAudioProducer audio_producer_;
		MultiJoystickMachine joystick_machine_;
		MultiKeyboardMachine keyboard_machine_;
		MultiMediaTarget media_target_;

		void pick_first();
		bool has_picked_ = false;
};

}
}

#endif /* MultiMachine_hpp */
