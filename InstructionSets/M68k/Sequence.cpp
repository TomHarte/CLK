//
//  Sequence.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Sequence.hpp"

using namespace InstructionSet::M68k;

template <typename EnumT, EnumT... T> struct Steps {
	static constexpr uint16_t value = 0;
};

template <typename EnumT, EnumT F, EnumT... T> struct Steps<EnumT, F, T...> {
	static constexpr uint16_t value = uint16_t(F) | uint16_t(Mask<EnumT, T...>::value << 3);
};

uint16_t Sequence::steps_for(Operation operation) {
	switch(operation) {
		// This handles a NOP, and not much else.
		default: return 0;

		case Operation::ABCD:	return Steps< Step::FetchOp1, Step::Perform, Step::StoreOp1 >::value;
	}
}

Sequence::Sequence(Operation operation) : steps_(steps_for(operation)) {}
