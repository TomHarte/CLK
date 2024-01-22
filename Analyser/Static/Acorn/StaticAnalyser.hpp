//
//  AcornAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::Acorn {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
