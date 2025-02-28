//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::AppleII {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
