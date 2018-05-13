//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Coleco_StaticAnalyser_hpp
#define StaticAnalyser_Coleco_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Coleco {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}


#endif /* StaticAnalyser_hpp */
