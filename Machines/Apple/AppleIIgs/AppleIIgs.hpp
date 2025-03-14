//
//  AppleIIgs.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Apple::IIgs {

struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns an AppleIIgs.
	static std::unique_ptr<Machine> AppleIIgs(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);
};

}
