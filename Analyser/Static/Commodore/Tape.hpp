//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Storage/Tape/Tape.hpp"
#include "File.hpp"

namespace Analyser::Static::Commodore {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &);

}
