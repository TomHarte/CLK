//
//  CommodoreAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_StaticAnalyser_hpp
#define StaticAnalyser_Commodore_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Commodore {

void AddTargets(const Media &media, std::vector<std::unique_ptr<Target>> &destination, const std::string &file_name);

}
}
}

#endif /* CommodoreAnalyser_hpp */
