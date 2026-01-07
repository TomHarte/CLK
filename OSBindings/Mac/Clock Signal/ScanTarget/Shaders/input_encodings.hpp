//
//  input_encodings.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "interpolators.hpp"
#include <metal_stdlib>

// Note to future self:
//
// To add a new InputEncoding, add it to the enum and #define below, set the semantic and input
// data formats below, and then implement the relevant sampler over in Samplers.metal. That should be it.
// Appropriate fragment logic will be synthesised from the declared semantic and data formats.

enum class InputEncoding {
	Luminance1,
	Luminance8,
	PhaseLinkedLuminance8,

	Luminance8Phase8,

	Red8Green8Blue8,
	Red4Green4Blue4,
	Red2Green2Blue2,
	Red1Green1Blue1,
};

#define AllEncodings(x)			\
	x(Luminance1);				\
	x(Luminance8);				\
	x(PhaseLinkedLuminance8);	\
	x(Luminance8Phase8);		\
	x(Red8Green8Blue8);			\
	x(Red4Green4Blue4);			\
	x(Red2Green2Blue2);			\
	x(Red1Green1Blue1);

/// @returns`true` if `encoding` produces only binary values.
/// This is used elsewhere potentially to avoid gamma correction — both 0 and 1 map to 0 and 1 regardless of gamma curve.
constexpr bool is_ttl(const InputEncoding encoding) {
	return encoding == InputEncoding::Luminance1 || encoding == InputEncoding::Red1Green1Blue1;
}

// Define the type of data captured by each input encoding.
using Luminance = half;					// i.e. a single sample of video, albeit potentially composite.
using LuminanceChrominance = half2;		// i.e. a single sample of s-video; .x = luminance; .y = chroma.
using RGB = half3;						// Standard semantics: .xyz = RGB.

template <InputEncoding> struct SemanticFormat { using type = RGB; };
template <> struct SemanticFormat<InputEncoding::Luminance8Phase8> { using type = LuminanceChrominance; };
template <> struct SemanticFormat<InputEncoding::Luminance1> { using type = Luminance; };
template <> struct SemanticFormat<InputEncoding::Luminance8> { using type = Luminance; };
template <> struct SemanticFormat<InputEncoding::PhaseLinkedLuminance8> { using type = Luminance; };
template <InputEncoding encoding> using semantic_t = typename SemanticFormat<encoding>::type;

// Define the per-pixel type of input textures based on data format.
template <InputEncoding> struct SampleDataType { using type = half; };
template<> struct SampleDataType<InputEncoding::Luminance1> { using type = ushort; };
template<> struct SampleDataType<InputEncoding::Red4Green4Blue4> { using type = ushort; };
template<> struct SampleDataType<InputEncoding::Red2Green2Blue2> { using type = ushort; };
template<> struct SampleDataType<InputEncoding::Red1Green1Blue1> { using type = ushort; };
template <InputEncoding encoding> using sample_t = typename SampleDataType<encoding>::type;
template <InputEncoding encoding> using texture_t = metal::texture2d<sample_t<encoding>>;

// Hence define a sampler for all texture types.
template <InputEncoding encoding> semantic_t<encoding> sample(SourceInterpolator, texture_t<encoding>);
