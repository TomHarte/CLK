//
//  MultiProducer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiProducer.hpp"

#include <atomic>

using namespace Analyser::Dynamic;

// MARK: - MultiInterface

template <typename MachineType>
void MultiInterface<MachineType>::perform_parallel(const std::function<void(MachineType *)> &function) {
	// Apply a blunt force parallelisation of the machines; each run_for is dispatched
	// to a separate queue and this queue will block until all are done.
	std::atomic_flag finished = false;
	{
		std::size_t outstanding_machines = machines_.size();

		for(std::size_t index = 0; index < machines_.size(); ++index) {
			const auto machine = ::Machine::get<MachineType>(*machines_[index].get());
			queues_[index].enqueue([&finished, machine, function, &outstanding_machines]() {
				if(machine) function(machine);

				--outstanding_machines;
				if(!outstanding_machines) {
//					finished.test_and_set();
				}
			});
		}
	}

	finished.wait(false, std::memory_order_relaxed);
}

template <typename MachineType>
void MultiInterface<MachineType>::perform_serial(const std::function<void(MachineType *)> &function) {
	std::lock_guard machines_lock(machines_mutex_);
	for(const auto &machine: machines_) {
		const auto typed_machine = ::Machine::get<MachineType>(*machine.get());
		if(typed_machine) function(typed_machine);
	}
}

// MARK: - MultiScanProducer
void MultiScanProducer::set_scan_target(Outputs::Display::ScanTarget *const scan_target) {
	scan_target_ = scan_target;

	std::lock_guard machines_lock(machines_mutex_);
	const auto machine = machines_.front()->scan_producer();
	if(machine) machine->set_scan_target(scan_target);
}

Outputs::Display::ScanStatus MultiScanProducer::get_scan_status() const {
	std::lock_guard machines_lock(machines_mutex_);
	const auto machine = machines_.front()->scan_producer();
	if(machine) return machine->get_scan_status();
	return Outputs::Display::ScanStatus();
}

void MultiScanProducer::did_change_machine_order() {
	if(scan_target_) scan_target_->will_change_owner();

	perform_serial([](MachineTypes::ScanProducer *machine) {
		machine->set_scan_target(nullptr);
	});
	std::lock_guard machines_lock(machines_mutex_);
	const auto machine = machines_.front()->scan_producer();
	if(machine) machine->set_scan_target(scan_target_);
}

// MARK: - MultiAudioProducer
MultiAudioProducer::MultiAudioProducer(
	const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines,
	std::recursive_mutex &machines_mutex
) :
	MultiInterface(machines, machines_mutex)
{
	speaker_ = MultiSpeaker::create(machines);
}

Outputs::Speaker::Speaker *MultiAudioProducer::get_speaker() {
	return speaker_;
}

void MultiAudioProducer::did_change_machine_order() {
	if(speaker_) {
		speaker_->set_new_front_machine(machines_.front().get());
	}
}

// MARK: - MultiTimedMachine

void MultiTimedMachine::run_for(const Time::Seconds duration) {
	perform_parallel([duration](::MachineTypes::TimedMachine *machine) {
		if(machine->get_confidence() >= 0.01f) machine->run_for(duration);
	});

	if(delegate_) delegate_->did_run_machines(*this);
}
