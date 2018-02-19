//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_Disk_hpp
#define StaticAnalyser_Commodore_Disk_hpp

#include "../../../Storage/Disk/Disk.hpp"
#include "File.hpp"

#include <vector>

namespace Analyser {
namespace Static {
namespace Commodore {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Disk::Disk> &disk);

}
}
}

#endif /* Disk_hpp */
