//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2019.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::PCCompatible {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
