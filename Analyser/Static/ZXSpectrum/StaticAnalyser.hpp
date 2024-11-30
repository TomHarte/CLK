//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser::Static::ZXSpectrum {

TargetList GetTargets(const Media &, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
