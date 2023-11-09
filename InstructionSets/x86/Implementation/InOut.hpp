//
//  InOut.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef InOut_h
#define InOut_h

#include "../AccessType.hpp"

namespace InstructionSet::x86::Primitive {

template <typename IntT, typename ContextT>
void out(
	uint16_t port,
	read_t<IntT> value,
	ContextT &context
) {
	context.io.template out<IntT>(port, value);
}

template <typename IntT, typename ContextT>
void in(
	uint16_t port,
	write_t<IntT> value,
	ContextT &context
) {
	value = context.io.template in<IntT>(port);
}

}

#endif /* InOut_h */
