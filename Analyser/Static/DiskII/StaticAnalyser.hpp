//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::DiskII {

TargetList GetTargets(const Media &, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
