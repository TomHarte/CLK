//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/TargetPlatforms.hpp"
#include "File.hpp"

namespace Analyser::Static::Commodore {

std::vector<File> GetFiles(Storage::Tape::TapeSerialiser &, TargetPlatform::Type);

}
