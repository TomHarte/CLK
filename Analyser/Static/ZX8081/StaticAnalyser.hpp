//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_ZX8081_StaticAnalyser_hpp
#define Analyser_Static_ZX8081_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace ZX8081 {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* StaticAnalyser_hpp */
