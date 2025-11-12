//
//  BiquadFilter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace SignalProcessing {

class BiquadFilter {
public:
	BiquadFilter() {
		// Default construction: no filter.
		coefficients_[0] = int16_t(1 << 15);
	}

	int16_t apply(const int16_t input) {
		const int16_t output = (
			coefficients_[0] * input +
			coefficients_[1] * inputs_[0] +
			coefficients_[2] * inputs_[1] +
			coefficients_[3] * outputs_[0] +
			coefficients_[4] * outputs_[1]
		) >> 15;

		inputs_[1] = inputs_[0];
		inputs_[0] = input;
		outputs_[1] = outputs_[0];
		outputs_[0] = output;

		return output;
	}

private:
	int16_t inputs_[2]{};
	int16_t outputs_[2]{};
	int16_t coefficients_[5]{};
		// 0 = b0; 1 = b1; 2 = b2; 3 = a1; 4 = a2
};

}
