//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::AmstradCPC {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
