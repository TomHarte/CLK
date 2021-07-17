//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Amiga_StaticAnalyser_hpp
#define Analyser_Static_Amiga_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Amiga {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}


#endif /* Analyser_Static_Amiga_StaticAnalyser_hpp */
