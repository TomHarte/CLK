//
//  MultiResettable.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MultiResettable.hpp"

using namespace Analyser::Dynamic;

MultiSoftResettable::MultiSoftResettable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(auto &machine : machines) {
		auto *const resettable = machine->soft_resettable();
		if(resettable) machines_.push_back(resettable);
	}
}

bool MultiSoftResettable::empty() const {
	return machines_.empty();
}

void MultiSoftResettable::soft_reset() {
	for(auto &machine : machines_) {
		machine->soft_reset();
	}
}

MultiHardResettable::MultiHardResettable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(auto &machine : machines) {
		auto *const resettable = machine->hard_resettable();
		if(resettable) machines_.push_back(resettable);
	}
}

bool MultiHardResettable::empty() const {
	return machines_.empty();
}

void MultiHardResettable::hard_reset() {
	for(auto &machine : machines_) {
		machine->hard_reset();
	}
}
