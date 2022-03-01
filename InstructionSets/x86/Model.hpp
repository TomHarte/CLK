//
//  Model.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef Model_h
#define Model_h

namespace InstructionSet {
namespace x86 {

enum class Model {
	i8086,
	i80186,
	i80286,
	i80386,
};

static constexpr bool is_32bit(Model model) { return model >= Model::i80386; }

}
}

#endif /* Model_h */
