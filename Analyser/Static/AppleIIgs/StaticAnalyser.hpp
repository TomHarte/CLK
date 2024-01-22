//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::AppleIIgs {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
