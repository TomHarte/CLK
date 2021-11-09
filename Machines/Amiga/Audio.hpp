//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Audio_hpp
#define Audio_hpp

#include "DMADevice.hpp"

namespace Amiga {

class Audio: public DMADevice<4> {
	public:
		using DMADevice::DMADevice;

		bool advance(int channel);

		void set_length(int, uint16_t);
		void set_period(int, uint16_t);
		void set_volume(int, uint16_t);
		void set_data(int, uint16_t);
};

}

#endif /* Audio_hpp */
