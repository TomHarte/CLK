//
//  CommodoreAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_StaticAnalyser_hpp
#define StaticAnalyser_Commodore_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Commodore {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* CommodoreAnalyser_hpp */
