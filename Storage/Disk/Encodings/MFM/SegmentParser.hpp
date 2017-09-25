//
//  SegmentParser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef SegmentParser_hpp
#define SegmentParser_hpp

#include "Sector.hpp"
#include "../../Track/PCMSegment.hpp"
#include <map>

namespace Storage {
namespace Encodings {
namespace MFM {

std::map<Sector::Address, Sector> SectorsFromSegment(const Disk::PCMSegment &&segment);

}
}
}

#endif /* SegmentParser_hpp */
