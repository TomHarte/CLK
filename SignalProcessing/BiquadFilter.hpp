//
//  BiquadFilter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>
#include <cmath>
#include <numbers>

namespace SignalProcessing {

/*!
	A biquad[ratic] filter approximates the real analogue thing in taking a 1d PCM signal and applying a
	filter to it as a function of the current input plus the two most-recent inputs plus the two most-recent outputs.

	So both IIR and three-tap FIR filters are degenerate cases of the biquad.

	It is used quite often in real designs, hence an implementation of this filter specifically.

	... and the below is largely textbook; I can't claim any great knowledge. I am especially indebted to
	the W3C Group's audio EQ cookbook at https://www.w3.org/TR/audio-eq-cookbook/ .
*/
class BiquadFilter {
public:
	enum class Type {
		LowPass,
		HighPass,
		BandPass,
		Notch,
		AllPass,
		Peaking,
		LowShelf,
		HighShelf
	};

	// Default construction: a filter that produces _nothing_.
	BiquadFilter() {}

	BiquadFilter(
		const Type type,
		const float sample_rate,
		const float frequency,
		const float resonance = 0.707f,
		const float gain = 8,
		const bool normalise = true
	) {
		configure(type, sample_rate, frequency, resonance, gain, normalise);
	}

	void configure(
		const Type type,
		const float sample_rate,
		const float frequency,
		const float resonance = 0.707f,
		const float gain = 8,
		const bool normalise = true
	) {
		const float w0 = 2.0f * std::numbers::pi_v<float> * frequency / sample_rate;
		const float alpha = std::sin(w0) / (2.0f * resonance);
		const float cos_w0 = std::cos(w0);

		float coefficients[5];
		float magnitude = 1.0f;
		switch(type) {
			case Type::LowPass:
				coefficients[0] = (1.0f - cos_w0) / 2.0f;
				coefficients[1] = 1.0f - cos_w0;
				coefficients[2] = (1.0f - cos_w0) / 2.0f;
				magnitude = 1.0f + alpha;
				coefficients[3] = -2.0f * cos_w0;
				coefficients[4] = 1.0f - alpha;
			break;

			case Type::HighPass:
				coefficients[0] = (1.0f - cos_w0) / 2.0f;
				coefficients[1] = -(1.0f + cos_w0);
				coefficients[2] = (1.0f - cos_w0) / 2.0f;
				magnitude = 1.0f + alpha;
				coefficients[3] = -2.0f * cos_w0;
				coefficients[4] = 1.0f - alpha;
			break;

			case Type::BandPass:
				coefficients[0] = alpha;
				coefficients[1] = 0.0f;
				coefficients[2] = -alpha;
				magnitude = 1.0f + alpha;
				coefficients[3] = -2.0f * cos_w0;
				coefficients[0] = 1.0f - alpha;
			break;

			case Type::Notch:
				coefficients[0] = 1.0f;
				coefficients[1] = -2.0f * cos_w0;
				coefficients[2] = 1.0f;
				magnitude = 1.0f + alpha;
				coefficients[3] = -2.0f * cos_w0;
				coefficients[4] = 1.0f - alpha;
			break;

			case Type::AllPass:
				coefficients[0] = 1.0f - alpha;
				coefficients[1] = -2.0f * cos_w0;
				coefficients[2] = 1.0f + alpha;
				magnitude = 1.0f + alpha;
				coefficients[3] = -2.0f * cos_w0;
				coefficients[4] = 1.0f - alpha;
			break;

			case Type::Peaking: {
				const float a = std::pow(10.0f, gain / 40.0f);

				coefficients[0] = 1.0f + (alpha * a);
				coefficients[1] = -2.0f * cos_w0;
				coefficients[2] = 1.0f - (alpha * a);
				magnitude = 1.0f + (alpha / a);
				coefficients[3] = -2.0f * cos_w0;
				coefficients[4] = 1.0f - (alpha / a);
			} break;

			case Type::LowShelf: {
				const float a_ls = std::pow(10.0f, gain / 40.0f);
				const float sqrt_a = std::sqrt(a_ls);
				const float alpha_ls =
					std::sin(w0) / 2.0f * std::sqrt((a_ls + 1.0f / a_ls) * (1.0f / resonance - 1.0f) + 2.0f);

				coefficients[0] = a_ls * ((a_ls + 1.0f) - (a_ls - 1.0f) * cos_w0 + 2.0f * sqrt_a * alpha_ls);
				coefficients[1] = 2.0f * a_ls * ((a_ls - 1.0f) - (a_ls + 1.0f) * cos_w0);
				coefficients[2] = a_ls * ((a_ls + 1.0f) - (a_ls - 1.0f) * cos_w0 - 2.0f * sqrt_a * alpha_ls);
				magnitude = (a_ls + 1.0f) + (a_ls - 1.0f) * cos_w0 + 2.0f * sqrt_a * alpha_ls;
				coefficients[3] = -2.0f * ((a_ls - 1) + (a_ls + 1) * cos_w0);
				coefficients[4] = (a_ls + 1.0f) + (a_ls - 1.0f) * cos_w0 - 2.0f * sqrt_a * alpha_ls;
			} break;

			case Type::HighShelf: {
				const float a_hs = std::pow(10.0f, gain / 40.0f);
				const float sqrt_a_hs = std::sqrt(a_hs);
				const float alpha_hs =
					std::sin(w0) / 2.0f * std::sqrt((a_hs + 1.0f / a_hs) * (1.0f / resonance - 1.0f) + 2.0f);

				coefficients[0] = a_hs * ((a_hs + 1.0f) + (a_hs - 1.0f) * cos_w0 + 2.0f * sqrt_a_hs * alpha_hs);
				coefficients[1] = -2.0f * a_hs * ((a_hs - 1.0f) + (a_hs + 1.0f) * cos_w0);
				coefficients[2] = a_hs * ((a_hs + 1.0f) + (a_hs - 1.0f) * cos_w0 - 2.0f * sqrt_a_hs * alpha_hs);
				magnitude = (a_hs + 1.0f) - (a_hs - 1.0f) * cos_w0 + 2.0f * sqrt_a_hs * alpha_hs;
				coefficients[3] = 2.0f * ((a_hs - 1.0f) - (a_hs + 1.0f) * cos_w0);
				coefficients[4] = (a_hs + 1.0f) - (a_hs - 1.0f) * cos_w0 - 2.0f * sqrt_a_hs * alpha_hs;
			} break;
		}

		if(normalise) {
			for(int c = 0; c < 5; c++) {
				coefficients[c] /= magnitude;
			}
		}
		for(int c = 0; c < 5; c++) {
#ifdef FIXED
			coefficients_[c] = FixedType(coefficients[c] * FixedMultiplier);
#else
			coefficients_[c] = coefficients[c];
#endif
		}
	}

	int16_t apply(const int16_t input) {
#ifdef FIXED
		const auto applied =
			coefficients_[0] * input +
			coefficients_[1] * inputs_[0] +
			coefficients_[2] * inputs_[1] -
			coefficients_[3] * outputs_[0] -
			coefficients_[4] * outputs_[1];

		const auto output = int16_t(applied >> FixedShift);
#else
		const float output =
			coefficients_[0] * float(input) +
			coefficients_[1] * inputs_[0] +
			coefficients_[2] * inputs_[1] -
			coefficients_[3] * outputs_[0] -
			coefficients_[4] * outputs_[1];
#endif

		inputs_[1] = inputs_[0];
		inputs_[0] = input;
		outputs_[1] = outputs_[0];
		outputs_[0] = output;

		return int16_t(output);
	}

private:
#ifdef FIXED
	int16_t inputs_[2]{};
	int16_t outputs_[2]{};

	using FixedType = int64_t;
	static constexpr int FixedShift = 48;
	static constexpr auto FixedMultiplier = static_cast<float>(int64_t(1) << FixedShift);
	FixedType coefficients_[5]{};
#else
	float inputs_[2]{};
	float outputs_[2]{};
	float coefficients_[5]{};
#endif
		// Coefficients indices versus common textbook terms:
		//	0 = b0; 1 = b1; 2 = b2; 3 = a1; 4 = a2
};

}
