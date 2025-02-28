//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Amiga {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
