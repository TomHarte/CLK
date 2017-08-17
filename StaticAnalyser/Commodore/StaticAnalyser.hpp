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

namespace StaticAnalyser {
namespace Commodore {

void AddTargets(const Media &media, std::list<Target> &destination);

}
}

#endif /* CommodoreAnalyser_hpp */
