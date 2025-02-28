//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::MSX {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
