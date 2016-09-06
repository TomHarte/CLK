//
//  Utilities.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Commodore_Utilities_hpp
#define Analyser_Commodore_Utilities_hpp

#include <string>

namespace StaticAnalyser {
namespace Commodore {

std::wstring petscii_from_bytes(const uint8_t *string, int length, bool shifted);

}
}

#endif /* Utilities_hpp */
