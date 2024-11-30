//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Coleco {

TargetList GetTargets(const Media &, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
