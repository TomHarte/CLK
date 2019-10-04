//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Atari_StaticAnalyser_hpp
#define StaticAnalyser_Atari_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Atari2600 {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* StaticAnalyser_hpp */
