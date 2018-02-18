//
//  MultiCRTMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MultiCRTMachine.hpp"

#include <condition_variable>
#include <mutex>

using namespace Analyser::Dynamic;

MultiCRTMachine::MultiCRTMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines, std::mutex &machines_mutex) :
	machines_(machines), machines_mutex_(machines_mutex), queues_(machines.size()) {
	speaker_ = MultiSpeaker::create(machines);
}

void MultiCRTMachine::perform_parallel(const std::function<void(::CRTMachine::Machine *)> &function) {
	// Apply a blunt force parallelisation of the machines; each run_for is dispatched
	// to a separate queue and this queue will block until all are done.
	volatile std::size_t outstanding_machines;
	std::condition_variable condition;
	std::mutex mutex;
	{
		std::lock_guard<std::mutex> machines_lock(machines_mutex_);
		std::lock_guard<std::mutex> lock(mutex);
		outstanding_machines = machines_.size();

		for(std::size_t index = 0; index < machines_.size(); ++index) {
			CRTMachine::Machine *crt_machine = machines_[index]->crt_machine();
			queues_[index].enqueue([&mutex, &condition, crt_machine, function, &outstanding_machines]() {
				if(crt_machine) function(crt_machine);

				{
					std::lock_guard<std::mutex> lock(mutex);
					outstanding_machines--;
				}
				condition.notify_all();
			});
		}
	}

	std::unique_lock<std::mutex> lock(mutex);
	if(outstanding_machines) condition.wait(lock, [&outstanding_machines] { return !outstanding_machines; });
}

void MultiCRTMachine::perform_serial(const std::function<void (::CRTMachine::Machine *)> &function) {
	std::lock_guard<std::mutex> machines_lock(machines_mutex_);
	for(const auto &machine: machines_) {
		CRTMachine::Machine *crt_machine = machine->crt_machine();
		if(crt_machine) function(crt_machine);
	}
}

void MultiCRTMachine::setup_output(float aspect_ratio) {
	perform_serial([=](::CRTMachine::Machine *machine) {
		machine->setup_output(aspect_ratio);
	});
}

void MultiCRTMachine::close_output() {
	perform_serial([=](::CRTMachine::Machine *machine) {
		machine->close_output();
	});
}

Outputs::CRT::CRT *MultiCRTMachine::get_crt() {
	std::lock_guard<std::mutex> machines_lock(machines_mutex_);
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_crt() : nullptr;
}

Outputs::Speaker::Speaker *MultiCRTMachine::get_speaker() {
	return speaker_;
}

void MultiCRTMachine::run_for(const Cycles cycles) {
	perform_parallel([=](::CRTMachine::Machine *machine) {
		if(machine->get_confidence() >= 0.01f) machine->run_for(cycles);
	});

	if(delegate_) delegate_->multi_crt_did_run_machines();
}

double MultiCRTMachine::get_clock_rate() {
	// TODO: something smarter than this? Not all clock rates will necessarily be the same.
	std::lock_guard<std::mutex> machines_lock(machines_mutex_);
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_clock_rate() : 0.0;
}

bool MultiCRTMachine::get_clock_is_unlimited() {
	std::lock_guard<std::mutex> machines_lock(machines_mutex_);
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_clock_is_unlimited() : false;
}

void MultiCRTMachine::did_change_machine_order() {
	if(speaker_) {
		std::lock_guard<std::mutex> machines_lock(machines_mutex_);
		speaker_->set_new_front_machine(machines_.front().get());
	}
}

void MultiCRTMachine::set_delegate(::CRTMachine::Machine::Delegate *delegate) {
	// TODO: 
}

void MultiCRTMachine::machine_did_change_clock_rate(Machine *machine) {
	// TODO: consider passing along.
}

void MultiCRTMachine::machine_did_change_clock_is_unlimited(Machine *machine) {
	// TODO: consider passing along.
}
