//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_Tape_hpp
#define StaticAnalyser_Acorn_Tape_hpp

#include <memory>

#include "File.hpp"
#include "../../../Storage/Tape/Tape.hpp"

namespace Analyser {
namespace Static {
namespace Acorn {

std::vector<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}
}

#endif /* Tape_hpp */
