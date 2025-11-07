//
//  TubeProcessor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/Acorn/Target.hpp"

namespace Acorn::Tube {

using TubeProcessor = Analyser::Static::Acorn::BBCMicroTarget::TubeProcessor;
template <typename ULAT, TubeProcessor> class Processor;

}
