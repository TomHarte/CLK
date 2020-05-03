//
//  KeyLevelScaler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef KeyLevelScaler_h
#define KeyLevelScaler_h

namespace Yamaha {
namespace OPL {

template <int frequency_precision> class KeyLevelScaler {
	public:

		/*!
			Sets the current period associated with the channel that owns this envelope generator;
			this is used to select a key scaling rate if key-rate scaling is enabled.
		*/
		void set_period(int period, int octave) {
			constexpr int key_level_scales[16] = {0, 48, 64, 74, 80, 86, 90, 94, 96, 100, 102, 104, 106, 108, 110, 112};
			constexpr int masks[2] = {~0, 0};

			// A two's complement assumption is embedded below; the use of masks relies
			// on the sign bit to clamp to zero.
			level_ = key_level_scales[period >> (frequency_precision - 4)];
			level_ -= 16 * (octave ^ 7);
			level_ &= masks[(key_scale_rate_ >> ((sizeof(int) * 8) - 1)) & 1];
		}

		/*!
			Enables or disables key-rate scaling.
		*/
		void set_key_scaling_level(int level) {
			// '7' is just a number large enough to render all possible scaling coefficients as 0.
			constexpr int key_level_scale_shifts[4] = {7, 1, 2, 0};
			shift_ = key_level_scale_shifts[level];
		}

		/*!
			@returns The current attenuation level due to key-level scaling.
		*/
		int attenuation() const {
			return level_ >> shift_;
		}

	private:
		int level_ = 0;
		int shift_ = 0;
};


}
}

#endif /* KeyLevelScaler_h */
