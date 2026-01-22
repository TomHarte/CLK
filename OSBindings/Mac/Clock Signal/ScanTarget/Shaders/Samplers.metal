//
//  Samplers.metal
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "input_encodings.hpp"

#include <metal_stdlib>

namespace {
constexpr metal::sampler standardSampler(
	metal::coord::pixel,
	metal::address::clamp_to_edge,
	metal::filter::nearest
);
}

// Luminance 1: each sample is either 0 or 1, representing a luminance of 0 or 1.
template <>
Luminance sample<InputEncoding::Luminance1>(
	const SourceInterpolator vert,
	const metal::texture2d<ushort> texture
) {
	return metal::clamp(half(texture.sample(standardSampler, vert.textureCoordinates).r), half(0.0f), half(1.0f));
}
template <> Luminance sample<InputEncoding::Luminance1>(SourceInterpolator, metal::texture2d<ushort>);

// Luminance 8: each sample is an 8-bit quantity representing the linear output level.
template <>
Luminance sample<InputEncoding::Luminance8>(
	const SourceInterpolator vert,
	const metal::texture2d<half> texture
) {
	return texture.sample(standardSampler, vert.textureCoordinates).r;
}
template <> Luminance sample<InputEncoding::Luminance8>(SourceInterpolator, metal::texture2d<half>);

// Phase-linked Luminance 8: each sample is four 8-bit quantities, with that representing the current
// output being selected according to its current phase.
template <>
Luminance sample<InputEncoding::PhaseLinkedLuminance8>(
	const SourceInterpolator vert,
	const metal::texture2d<half> texture
) {
	const int offset = int(vert.unitColourPhase * 4.0f) & 3;
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates);
	return sample[offset];
}
template <> Luminance sample<InputEncoding::PhaseLinkedLuminance8>(SourceInterpolator, metal::texture2d<half>);

// Luminance 8, Phase 8: each sample is two 8-bit quantities; the first is a luminance and the second is the
// phase offset of a perfect cosine curve within this sample.
template <>
LuminanceChrominance sample<InputEncoding::Luminance8Phase8>(
	const SourceInterpolator vert,
	const metal::texture2d<half> texture
) {
	const auto luminancePhase = texture.sample(standardSampler, vert.textureCoordinates).rg;
	const half phaseOffset = 3.141592654 * 4.0 * luminancePhase.g;
	const half rawChroma = metal::step(luminancePhase.g, half(0.75f)) * metal::cos(vert.colourPhase + phaseOffset);
	return half2(luminancePhase.r, rawChroma);
}
template <> LuminanceChrominance sample<InputEncoding::Luminance8Phase8>(SourceInterpolator, metal::texture2d<half>);

// Red 8, Green 8, Blue 8: each sample is an RGB value with 8 bits per channel.
template<> RGB sample<InputEncoding::Red8Green8Blue8>(
	const SourceInterpolator vert,
	const metal::texture2d<half> texture
) {
	return texture.sample(standardSampler, vert.textureCoordinates).rgb;
}
template <> RGB sample<InputEncoding::Red8Green8Blue8>(SourceInterpolator, metal::texture2d<half>);

// Red 4, Green 4, Blue 4: each sample is an RGB value with 4 bits per channel; the channels are packed into a 16-bit
// quantity with the layout xxxx RRRR GGGG BBBB.
template<> RGB sample<InputEncoding::Red4Green4Blue4>(
	const SourceInterpolator vert,
	const metal::texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).rg;
	return half3(sample.r&15, (sample.g >> 4)&15, sample.g&15) / 15.0f;
}
template <> RGB sample<InputEncoding::Red4Green4Blue4>(SourceInterpolator, metal::texture2d<ushort>);

// Red 2, Green 2, Blue 2: each sample is an RGB value with 2 bits per channel; the channels are packed into an 8-bit
// quantity with the layout xx RR GG BB.
template<> RGB sample<InputEncoding::Red2Green2Blue2>(
	const SourceInterpolator vert,
	const metal::texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return half3((sample >> 4)&3, (sample >> 2)&3, sample&3) / 3.0f;
}
template <> RGB sample<InputEncoding::Red2Green2Blue2>(SourceInterpolator, metal::texture2d<ushort>);

// Red 1, Green 1, Blue 1: each sample is an RGB value with 1 bit per channel; the channels are packed into an 8-bit
// quantity with the layout xxxx xRGB.
template<> RGB sample<InputEncoding::Red1Green1Blue1>(
	const SourceInterpolator vert,
	const metal::texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return metal::clamp(half3(sample&4, sample&2, sample&1), half(0.0f), half(1.0f));
}
template <> RGB sample<InputEncoding::Red1Green1Blue1>(SourceInterpolator, metal::texture2d<ushort>);
