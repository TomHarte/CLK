//
//  MultiSpeaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiSpeaker.hpp"

using namespace Analyser::Dynamic;

MultiSpeaker *MultiSpeaker::create(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	std::vector<Outputs::Speaker::Speaker *> speakers;
	for(const auto &machine: machines) {
		Outputs::Speaker::Speaker *speaker = machine->crt_machine()->get_speaker();
		if(speaker) speakers.push_back(speaker);
	}
	if(speakers.empty()) return nullptr;

	return new MultiSpeaker(speakers);
}

MultiSpeaker::MultiSpeaker(const std::vector<Outputs::Speaker::Speaker *> &speakers) :
	speakers_(speakers), front_speaker_(speakers.front()) {
	for(const auto &speaker: speakers_) {
		speaker->set_delegate(this);
	}
}

float MultiSpeaker::get_ideal_clock_rate_in_range(float minimum, float maximum) {
	float ideal = 0.0f;
	for(const auto &speaker: speakers_) {
		ideal += speaker->get_ideal_clock_rate_in_range(minimum, maximum);
	}

	return ideal / static_cast<float>(speakers_.size());
}

void MultiSpeaker::set_output_rate(float cycles_per_second, int buffer_size) {
	for(const auto &speaker: speakers_) {
		speaker->set_output_rate(cycles_per_second, buffer_size);
	}
}

void MultiSpeaker::set_delegate(Outputs::Speaker::Speaker::Delegate *delegate) {
	delegate_ = delegate;
}

void MultiSpeaker::speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) {
	if(!delegate_) return;
	{
		std::lock_guard<std::mutex> lock_guard(front_speaker_mutex_);
		if(speaker != front_speaker_) return;
	}
	did_complete_samples(this, buffer);
}

void MultiSpeaker::speaker_did_change_input_clock(Speaker *speaker) {
	if(!delegate_) return;
	{
		std::lock_guard<std::mutex> lock_guard(front_speaker_mutex_);
		if(speaker != front_speaker_) return;
	}
	delegate_->speaker_did_change_input_clock(this);
}

void MultiSpeaker::set_new_front_machine(::Machine::DynamicMachine *machine) {
	{
		std::lock_guard<std::mutex> lock_guard(front_speaker_mutex_);
		front_speaker_ = machine->crt_machine()->get_speaker();
	}
	if(delegate_) {
		delegate_->speaker_did_change_input_clock(this);
	}
}
