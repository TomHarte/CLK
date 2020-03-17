//
//  MultiConfigurable.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiConfigurable.hpp"

#include <algorithm>

using namespace Analyser::Dynamic;

MultiConfigurable::MultiConfigurable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		Configurable::Device *device = machine->configurable_device();
		if(device) devices_.push_back(device);
	}
}

void MultiConfigurable::set_options(const std::unique_ptr<Reflection::Struct> &options) {
}

std::unique_ptr<Reflection::Struct> MultiConfigurable::get_options() {
	// TODO: this'll need to mash options together, maybe? Or just take the front?
	return nullptr;
}

/*std::vector<std::unique_ptr<Configurable::Option>> MultiConfigurable::get_options() {
	std::vector<std::unique_ptr<Configurable::Option>> options;

	// Produce the list of unique options.
	for(const auto &device : devices_) {
		std::vector<std::unique_ptr<Configurable::Option>> device_options = device->get_options();
		for(auto &option : device_options) {
			if(std::find(options.begin(), options.end(), option) == options.end()) {
				options.push_back(std::move(option));
			}
		}
	}

	return options;
}

void MultiConfigurable::set_selections(const Configurable::SelectionSet &selection_by_option) {
	for(const auto &device : devices_) {
		device->set_selections(selection_by_option);
	}
}

Configurable::SelectionSet MultiConfigurable::get_accurate_selections() {
	Configurable::SelectionSet set;
	for(const auto &device : devices_) {
		Configurable::SelectionSet device_set = device->get_accurate_selections();
		for(auto &selection : device_set) {
			set.insert(std::move(selection));
		}
	}
	return set;
}

Configurable::SelectionSet MultiConfigurable::get_user_friendly_selections() {
	Configurable::SelectionSet set;
	for(const auto &device : devices_) {
		Configurable::SelectionSet device_set = device->get_user_friendly_selections();
		for(auto &selection : device_set) {
			set.insert(std::move(selection));
		}
	}
	return set;
}
*/
