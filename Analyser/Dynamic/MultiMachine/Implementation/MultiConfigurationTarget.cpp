//
//  MultiConfigurationTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiConfigurationTarget.hpp"

using namespace Analyser::Dynamic;

MultiConfigurationTarget::MultiConfigurationTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		ConfigurationTarget::Machine *configuration_target = machine->configuration_target();
		if(configuration_target) targets_.push_back(configuration_target);
	}
}

void MultiConfigurationTarget::configure_as_target(const Analyser::Static::Target *target) {
}

bool MultiConfigurationTarget::insert_media(const Analyser::Static::Media &media) {
	bool inserted = false;
	for(const auto &target : targets_) {
		inserted |= target->insert_media(media);
	}
	return inserted;
}
