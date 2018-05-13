//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_MSX_StaticAnalyser_hpp
#define StaticAnalyser_MSX_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace MSX {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* StaticAnalyser_MSX_StaticAnalyser_hpp */
