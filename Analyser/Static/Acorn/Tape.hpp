//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <memory>

#include "File.hpp"
#include "../../../Storage/Tape/Tape.hpp"

namespace Analyser::Static::Acorn {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &);

}
