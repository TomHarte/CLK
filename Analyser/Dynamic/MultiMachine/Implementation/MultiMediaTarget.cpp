//
//  MultiMediaTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiMediaTarget.hpp"
#include <unordered_set>

using namespace Analyser::Dynamic;

MultiMediaTarget::MultiMediaTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		auto media_target = machine->media_target();
		if(media_target) targets_.push_back(media_target);
	}
}

bool MultiMediaTarget::insert_media(const Analyser::Static::Media &media) {
	// TODO: copy media afresh for each target machine; media
	// generally has mutable state.

	bool inserted = false;
	for(const auto &target : targets_) {
		inserted |= target->insert_media(media);
	}
	return inserted;
}

MultiMediaChangeObserver::MultiMediaChangeObserver(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		auto media_change_observer = machine->media_change_observer();
		if(media_change_observer) targets_.push_back(media_change_observer);
	}
}

using ChangeEffect = MachineTypes::MediaChangeObserver::ChangeEffect;

ChangeEffect MultiMediaChangeObserver::effect_for_file_did_change(const std::string &name) const {
	if(targets_.empty()) {
		return ChangeEffect::None;
	}

	std::unordered_set<ChangeEffect> effects;
	for(const auto &target: targets_) {
		effects.insert(target->effect_for_file_did_change(name));
	}

	// No agreement => restart.
	if(effects.size() > 1) {
		return ChangeEffect::RestartMachine;
	}
	return *effects.begin();
}
