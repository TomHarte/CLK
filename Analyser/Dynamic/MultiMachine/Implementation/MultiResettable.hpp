//
//  MultiResettable.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/DynamicMachine.hpp"

namespace Analyser::Dynamic {

class MultiSoftResettable: public MachineTypes::SoftResettable {
public:
	MultiSoftResettable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &);
	bool empty() const;

	void soft_reset() final;

private:
	std::vector<MachineTypes::SoftResettable *> machines_;
};

class MultiHardResettable: public MachineTypes::HardResettable {
public:
	MultiHardResettable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &);
	bool empty() const;

	void hard_reset() final;

private:
	std::vector<MachineTypes::HardResettable *> machines_;

};

}
