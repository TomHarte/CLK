//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_AtariST_StaticAnalyser_hpp
#define Analyser_Static_AtariST_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace AtariST {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}


#endif /* Analyser_Static_AtariST_StaticAnalyser_hpp */
