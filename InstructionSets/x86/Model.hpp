//
//  Model.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

namespace InstructionSet::x86 {

enum class Model {
	i8086,
	i80186,
	i80286,
	i80386,
};

static constexpr bool is_32bit(Model model) { return model >= Model::i80386; }

template <bool is_32bit> struct AddressT { using type = uint16_t; };
template <> struct AddressT<true> { using type = uint32_t; };

}
