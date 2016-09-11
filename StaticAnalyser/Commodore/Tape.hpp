//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_Tape_hpp
#define StaticAnalyser_Commodore_Tape_hpp

#include <cstdint>
#include "../StaticAnalyser.hpp"
#include "File.hpp"

namespace StaticAnalyser {
namespace Commodore {

std::list<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}
#endif /* Tape_hpp */
