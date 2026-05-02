//
//  CoCo.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

namespace Tandy::CoCo {

struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> create(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);
};

}
