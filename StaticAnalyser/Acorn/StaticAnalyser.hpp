//
//  AcornAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_StaticAnalyser_hpp
#define StaticAnalyser_Acorn_StaticAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace Acorn {

void AddTargets(const Media &media, std::list<Target> &destination);

}
}

#endif /* AcornAnalyser_hpp */
