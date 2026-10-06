//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Storage/TargetPlatforms.hpp"

#include <filesystem>

namespace Analyser::Static::Thomson {

TargetList GetTargets(const Media &, const std::filesystem::path &, TargetPlatform::IntType, bool);

}
