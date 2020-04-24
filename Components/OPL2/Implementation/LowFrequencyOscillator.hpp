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
		/// A number between 0 and 7 indicating the current vibrato offset; this should be combined by operators
		/// with their frequency number to get the actual vibrato.
		int vibrato = 0;
		/// A counter of the number of operator update cycles (i.e. input clock / 72) since an arbitrary time.
		int counter = 0;

		/// Updates the oscillator outputs
		void update();
};

}
}

#endif /* LowFrequencyOscillator_hpp */
