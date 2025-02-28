//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "File.hpp"
#include "Storage/Tape/Tape.hpp"

#include <vector>

namespace Analyser::Static::Acorn {

std::vector<File> GetFiles(Storage::Tape::TapeSerialiser &);

}
