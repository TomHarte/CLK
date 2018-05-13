//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_AmstradCPC_StaticAnalyser_hpp
#define Analyser_Static_AmstradCPC_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace AmstradCPC {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* Analyser_Static_AmstradCPC_StaticAnalyser_hpp */
