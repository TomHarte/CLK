//
//  Model.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include "Mode.hpp"

namespace InstructionSet::x86 {

enum class Model {
	i8086,
	i80186,
	i80286,
	i80386,
};

enum class InstructionType {
	Bits16,
	Bits32,
};

template <InstructionType type> struct DisplacementT;
template<> struct DisplacementT<InstructionType::Bits16> { using type = int16_t; };
template<> struct DisplacementT<InstructionType::Bits32> { using type = int32_t; };

template <InstructionType type> struct ImmediateT;
template<> struct ImmediateT<InstructionType::Bits16> { using type = uint16_t; };
template<> struct ImmediateT<InstructionType::Bits32> { using type = uint32_t; };

template <InstructionType type> using AddressT = ImmediateT<type>;

static constexpr InstructionType instruction_type(const Model model) {
	return model >= Model::i80386 ? InstructionType::Bits32 : InstructionType::Bits16;
}

static constexpr bool has_mode(const Model model, const Mode mode) {
	switch(mode) {
		case Mode::Real:	return true;
		case Mode::Protected286:
			return model >= Model::i80286;
	}
	return false;
}

static constexpr bool uses_8086_exceptions(const Model model) {
	return model <= Model::i80186;
}


template <Model model>
concept has_descriptor_tables = model >= Model::i80286;

template <Model model>
concept has_32bit_instructions = model >= Model::i80386;

}
