//
//  MO5.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Thomson::MO5 {

struct Machine {
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> MO5(
		const Analyser::Static::Target *,
		const ROMMachine::ROMFetcher &
	);
};

}
