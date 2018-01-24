//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_MSX_StaticAnalyser_hpp
#define StaticAnalyser_MSX_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace MSX {

void AddTargets(const Media &media, std::vector<Target> &destination);

}
}

#endif /* StaticAnalyser_MSX_StaticAnalyser_hpp */
