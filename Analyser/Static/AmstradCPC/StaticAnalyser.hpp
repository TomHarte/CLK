//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_AmstradCPC_StaticAnalyser_hpp
#define StaticAnalyser_AmstradCPC_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace AmstradCPC {

void AddTargets(const Media &media, std::vector<std::unique_ptr<Target>> &destination);

}
}
}

#endif /* StaticAnalyser_AmstradCPC_StaticAnalyser_hpp */
