//
//  ImplicitSectors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ImplicitSectors_hpp
#define ImplicitSectors_hpp

#include "../../../Track/Track.hpp"
#include <memory>
#include <vector>

namespace Storage {
namespace Disk {

std::shared_ptr<Track> track_for_sectors(const uint8_t *source, int number_of_sectors, uint8_t track, uint8_t side, uint8_t first_sector, uint8_t size, bool is_double_density);
void decode_sectors(Track &track, uint8_t *destination, uint8_t first_sector, uint8_t last_sector, uint8_t sector_size, bool is_double_density);

}
}

#endif /* ImplicitSectors_hpp */
