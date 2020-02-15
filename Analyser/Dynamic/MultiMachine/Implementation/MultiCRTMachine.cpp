//
//  MultiCRTMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiCRTMachine.hpp"

#include <condition_variable>
#include <mutex>

using namespace Analyser::Dynamic;

MultiCRTMachine::MultiCRTMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines, std::recursive_mutex &machines_mutex) :
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
		std::lock_guard<decltype(machines_mutex_)> machines_lock(machines_mutex_);
		std::lock_guard<std::mutex> lock(mutex);
		outstanding_machines = machines_.size();

		for(std::size_t index = 0; index < machines_.size(); ++index) {
			CRTMachine::Machine *crt_machine = machines_[index]->crt_machine();
			queues_[index].enqueue([&mutex, &condition, crt_machine, function, &outstanding_machines]() {
				if(crt_machine) function(crt_machine);

				std::lock_guard<std::mutex> lock(mutex);
				outstanding_machines--;
				condition.notify_all();
			});
		}
	}

	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock, [&outstanding_machines] { return !outstanding_machines; });
}

void MultiCRTMachine::perform_serial(const std::function<void (::CRTMachine::Machine *)> &function) {
	std::lock_guard<decltype(machines_mutex_)> machines_lock(machines_mutex_);
	for(const auto &machine: machines_) {
		CRTMachine::Machine *const crt_machine = machine->crt_machine();
		if(crt_machine) function(crt_machine);
	}
}

void MultiCRTMachine::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	scan_target_ = scan_target;

	CRTMachine::Machine *const crt_machine = machines_.front()->crt_machine();
	if(crt_machine) crt_machine->set_scan_target(scan_target);
}

Outputs::Display::ScanStatus MultiCRTMachine::get_scan_status() const {
	CRTMachine::Machine *const crt_machine = machines_.front()->crt_machine();
	if(crt_machine) crt_machine->get_scan_status();

	return Outputs::Display::ScanStatus();
}

Outputs::Speaker::Speaker *MultiCRTMachine::get_speaker() {
	return speaker_;
}

void MultiCRTMachine::run_for(Time::Seconds duration) {
	perform_parallel([duration](::CRTMachine::Machine *machine) {
		if(machine->get_confidence() >= 0.01f) machine->run_for(duration);
	});

	if(delegate_) delegate_->multi_crt_did_run_machines();
}

void MultiCRTMachine::did_change_machine_order() {
	if(scan_target_) scan_target_->will_change_owner();

	perform_serial([](::CRTMachine::Machine *machine) {
		machine->set_scan_target(nullptr);
	});
	CRTMachine::Machine *const crt_machine = machines_.front()->crt_machine();
	if(crt_machine) crt_machine->set_scan_target(scan_target_);

	if(speaker_) {
		speaker_->set_new_front_machine(machines_.front().get());
	}
}
