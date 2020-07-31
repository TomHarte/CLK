//
//  MultiMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiMachine.hpp"
#include "../../../Outputs/Log.hpp"

#include <algorithm>

using namespace Analyser::Dynamic;

MultiMachine::MultiMachine(std::vector<std::unique_ptr<DynamicMachine>> &&machines) :
	machines_(std::move(machines)),
	configurable_(machines_),
	timed_machine_(machines_, machines_mutex_),
	scan_producer_(machines_, machines_mutex_),
	audio_producer_(machines_, machines_mutex_),
	joystick_machine_(machines_),
	keyboard_machine_(machines_),
	media_target_(machines_) {
	timed_machine_.set_delegate(this);
}

Activity::Source *MultiMachine::activity_source() {
	return nullptr; // TODO
}

#define Provider(type, name, member)	\
	type *MultiMachine::name() {	\
		if(has_picked_) {	\
			return machines_.front()->name();	\
		} else {	\
			return &member;	\
		}	\
	}

Provider(Configurable::Device, configurable_device, configurable_)
Provider(MachineTypes::TimedMachine, timed_machine, timed_machine_)
Provider(MachineTypes::ScanProducer, scan_producer, scan_producer_)
Provider(MachineTypes::AudioProducer, audio_producer, audio_producer_)
Provider(MachineTypes::JoystickMachine, joystick_machine, joystick_machine_)
Provider(MachineTypes::KeyboardMachine, keyboard_machine, keyboard_machine_)
Provider(MachineTypes::MediaTarget, media_target, media_target_)

MachineTypes::MouseMachine *MultiMachine::mouse_machine() {
	// TODO.
	return nullptr;
}

#undef Provider

bool MultiMachine::would_collapse(const std::vector<std::unique_ptr<DynamicMachine>> &machines) {
	return
		(machines.front()->timed_machine()->get_confidence() > 0.9f) ||
		(machines.front()->timed_machine()->get_confidence() >= 2.0f * machines[1]->timed_machine()->get_confidence());
}

void MultiMachine::did_run_machines(MultiTimedMachine *) {
	std::lock_guard machines_lock(machines_mutex_);
#ifndef NDEBUG
	for(const auto &machine: machines_) {
		auto timed_machine = machine->timed_machine();
		LOGNBR(PADHEX(2) << timed_machine->get_confidence() << " " << timed_machine->debug_type() << "; ");
	}
	LOGNBR(std::endl);
#endif

	DynamicMachine *front = machines_.front().get();
	std::stable_sort(machines_.begin(), machines_.end(),
		[] (const std::unique_ptr<DynamicMachine> &lhs, const std::unique_ptr<DynamicMachine> &rhs){
			auto lhs_timed = lhs->timed_machine();
			auto rhs_timed = rhs->timed_machine();
			return lhs_timed->get_confidence() > rhs_timed->get_confidence();
		});

	if(machines_.front().get() != front) {
		scan_producer_.did_change_machine_order();
		audio_producer_.did_change_machine_order();
	}

	if(would_collapse(machines_)) {
		pick_first();
	}
}

void MultiMachine::pick_first() {
	has_picked_ = true;

	// Ensure output rate specifics are properly copied; these may be set only once by the owner,
	// but rather than being propagated directly by the MultiSpeaker only the derived computed
	// output rate is propagated. So this ensures that if a new derivation is made, it's made correctly.
	if(machines_[0]->audio_producer()) {
		auto multi_speaker = audio_producer_.get_speaker();
		auto specific_speaker = machines_[0]->audio_producer()->get_speaker();

		if(specific_speaker && multi_speaker) {
			specific_speaker->copy_output_rate(*multi_speaker);
		}
	}

	// TODO: because it is not invalid for a caller to keep a reference to anything previously returned,
	// this erase can be added only once the Multi machines that take static copies of the machines list
	// are updated.
	//
	// Example failing use case otherwise: a caller still has reference to the MultiJoystickMachine, and
	// it has dangling references to the various JoystickMachines.
	//
	// This gets into particularly long grass with the MultiConfigurable and its MultiStruct.
//	machines_.erase(machines_.begin() + 1, machines_.end());
}

void *MultiMachine::raw_pointer() {
	return nullptr;
}
