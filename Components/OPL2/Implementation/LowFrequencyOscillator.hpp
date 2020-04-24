//
//  LowFrequencyOscillator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef LowFrequencyOscillator_hpp
#define LowFrequencyOscillator_hpp

namespace Yamaha {
namespace OPL {

/*!
	Models the output of the OPL low-frequency oscillator, which provides a couple of optional fixed-frequency
	modifications to an operator: tremolo and vibrato. Also exposes a global time counter, which oscillators use
	as part of their ADSR envelope.
*/
class LowFrequencyOscillator {
	public:
		/// Current attenuation due to tremolo / amplitude modulation, between 0 and 26.
		int tremolo = 0;
		/// TODO
		int vibrato = 0;
		/// A counter of the number of operator update cycles (i.e. input clock / 72) since an arbitrary time.
		int counter = 0;

		/// Updates the oscillator outputs
		void update() {
			++counter;

			// Update tremolo.
			++tremolo_phase_;

			// This produces output of:
			//
			// four instances of 0, four instances of 1... _three_ instances of 26,
			// four instances of 25, four instances of 24... _three_ instances of 0.
			//
			// ... advancing once every 64th update.
			const int tremolo_index = (tremolo_phase_ >> 6) % 210;
			const int tremolo_levels[2] = {tremolo_index >> 2, 52 - ((tremolo_index+1) >> 2)};
			tremolo = tremolo_levels[tremolo_index / 107];
		}

	private:
		int tremolo_phase_ = 0;
};

}
}

#endif /* LowFrequencyOscillator_hpp */
