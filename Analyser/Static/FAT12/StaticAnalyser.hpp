//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/12/2023.
//  Copyright 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::FAT12 {

TargetList GetTargets(const Media &, const std::string &, TargetPlatform::IntType, bool);

}
