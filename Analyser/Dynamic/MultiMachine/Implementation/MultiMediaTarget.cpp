//
//  MultiMediaTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiMediaTarget.hpp"

using namespace Analyser::Dynamic;

MultiMediaTarget::MultiMediaTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		auto media_target = machine->media_target();
		if(media_target) targets_.push_back(media_target);
	}
}

bool MultiMediaTarget::insert_media(const Analyser::Static::Media &media) {
	bool inserted = false;
	for(const auto &target : targets_) {
		inserted |= target->insert_media(media);
	}
	return inserted;
}
