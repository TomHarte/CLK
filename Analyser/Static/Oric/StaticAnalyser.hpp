//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Oric_StaticAnalyser_hpp
#define StaticAnalyser_Oric_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Oric {

void AddTargets(const Media &media, std::vector<Target> &destination);

}
}
}

#endif /* StaticAnalyser_hpp */
