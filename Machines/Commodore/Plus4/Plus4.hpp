//
//  Plus4.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Commodore::Plus4 {

class Machine {
public:
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> Plus4(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);
};

}
