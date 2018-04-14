//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_AppleII_StaticAnalyser_hpp
#define Analyser_Static_AppleII_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace AppleII {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* Analyser_Static_AppleII_StaticAnalyser_hpp */
