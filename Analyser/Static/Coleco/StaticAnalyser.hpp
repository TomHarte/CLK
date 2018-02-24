//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Coleco_StaticAnalyser_hpp
#define StaticAnalyser_Coleco_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Coleco {

void AddTargets(const Media &media, std::vector<std::unique_ptr<Target>> &destination);

}
}
}


#endif /* StaticAnalyser_hpp */
