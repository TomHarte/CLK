//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Oric {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
