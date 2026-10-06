//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"

#include <filesystem>

namespace Analyser::Static::Macintosh {

TargetList GetTargets(const Media &, const std::filesystem::path &, TargetPlatform::IntType, bool);

}
