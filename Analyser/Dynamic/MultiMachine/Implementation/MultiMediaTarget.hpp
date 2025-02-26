//
//  MultiMediaTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../../Machines/MediaTarget.hpp"
#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser::Dynamic {

/*!
	Provides a class that multiplexes the media target interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
struct MultiMediaTarget: public MachineTypes::MediaTarget {
public:
	MultiMediaTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &);

	// Below is the standard MediaTarget::Machine interface; see there for documentation.
	bool insert_media(const Analyser::Static::Media &) final;

private:
	std::vector<MachineTypes::MediaTarget *> targets_;
};

struct MultiMediaChangeObserver: public MachineTypes::MediaChangeObserver {
public:
	MultiMediaChangeObserver(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &);

	// Below is the standard MediaTarget::Machine interface; see there for documentation.
	ChangeEffect effect_for_file_did_change(const std::string &) const final;

private:
	std::vector<MachineTypes::MediaChangeObserver *> targets_;
};

}
