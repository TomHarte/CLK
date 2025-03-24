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

static constexpr bool is_32bit(const Model model) { return model >= Model::i80386; }
static constexpr bool has_mode(const Model model, const Mode mode) {
	switch(mode) {
		case Mode::Real:	return true;
		case Mode::Protected286:
			return model >= Model::i80286;
	}
	return false;
}

template <bool is_32bit> struct AddressT { using type = uint16_t; };
template <> struct AddressT<true> { using type = uint32_t; };

}
