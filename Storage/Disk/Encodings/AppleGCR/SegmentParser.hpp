//
//  SegmentParser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef TrackParser_hpp
#define TrackParser_hpp

#include "Sector.hpp"
#include "../../Track/PCMSegment.hpp"
#include <map>

namespace Storage {
namespace Encodings {
namespace AppleGCR {

/*!
	Scans @c segment for all included sectors, returning a set that maps from location within
	the segment (counted in bits from the beginning) to sector.
*/
std::map<std::size_t, Sector> sectors_from_segment(const Disk::PCMSegment &segment);

}
}
}

#endif /* TrackParser_hpp */
