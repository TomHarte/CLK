//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/06/2021.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Enterprise {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
