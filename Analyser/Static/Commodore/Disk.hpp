//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Storage/Disk/Disk.hpp"
#include "File.hpp"

#include <vector>

namespace Analyser::Static::Commodore {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Disk::Disk> &);

}
