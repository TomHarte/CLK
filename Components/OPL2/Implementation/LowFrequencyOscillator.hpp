//
//  LowFrequencyOscillator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef LowFrequencyOscillator_hpp
#define LowFrequencyOscillator_hpp

#include "../../../Numeric/LFSR.hpp"

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

		/// Describes the current output of the LFSR; will be either 0 or 1.
		int lfsr = 0;

		/// Updates the oscillator outputs. Should be called at the (input clock/72) rate.
		void update() {
			++counter;

			// This produces output of:
			//
			// four instances of 0, four instances of 1... _three_ instances of 26,
			// four instances of 25, four instances of 24... _three_ instances of 0.
			//
			// ... advancing once every 64th update.
			const int tremolo_index = (counter >> 6) % 210;
			const int tremolo_levels[2] = {tremolo_index >> 2, 52 - ((tremolo_index+1) >> 2)};
			tremolo = tremolo_levels[tremolo_index / 107];

			// Vibrato is relatively simple: it's just three bits from the counter.
			vibrato = (counter >> 10) & 7;
		}

		/// Updartes the LFSR output. Should be called at the input clock rate.
		void update_lfsr() {
			lfsr = noise_source_.next();		
		}

	private:
		// This is the correct LSFR per forums.submarine.org.uk.
		Numeric::LFSR<int, 0x800302> noise_source_;
};

}
}

#endif /* LowFrequencyOscillator_hpp */
