//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/06/2021.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Enterprise_StaticAnalyser_hpp
#define Analyser_Static_Enterprise_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Enterprise {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}


#endif /* Analyser_Static_Enterprise_StaticAnalyser_hpp */
