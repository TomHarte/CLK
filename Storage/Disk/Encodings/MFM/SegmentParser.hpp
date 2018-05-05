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

/*!
	Scans @c segment for all included sectors, returning a set that maps from location within
	the segment (counted in bits from the beginning and pointing to the location the disk
	had reached upon detection of the ID mark) to sector.
*/
std::map<std::size_t, Sector> sectors_from_segment(const Disk::PCMSegment &&segment, bool is_double_density);

}
}
}

#endif /* SegmentParser_hpp */
