//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_ZXSpectrum_StaticAnalyser_hpp
#define Analyser_Static_ZXSpectrum_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include "../../../Storage/TargetPlatforms.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace ZXSpectrum {

TargetList GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms);

}
}
}

#endif /* StaticAnalyser_hpp */
