//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_Tape_hpp
#define StaticAnalyser_Commodore_Tape_hpp

#include "../../../Storage/Tape/Tape.hpp"
#include "File.hpp"

namespace Analyser {
namespace Static {
namespace Commodore {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}
}

#endif /* Tape_hpp */
