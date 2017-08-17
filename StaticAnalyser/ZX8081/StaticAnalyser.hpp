//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_ZX8081_StaticAnalyser_hpp
#define StaticAnalyser_ZX8081_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace ZX8081 {

void AddTargets(const Media &media, std::list<Target> &destination);

}
}

#endif /* StaticAnalyser_hpp */
