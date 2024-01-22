//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Macintosh {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
