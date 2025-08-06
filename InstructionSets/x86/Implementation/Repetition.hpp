//
//  Repetition.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"

namespace InstructionSet::x86::Primitive {

template <typename AddressT, Repetition repetition>
bool repetition_over(
	const AddressT &eCX
) {
	return repetition != Repetition::None && !eCX;
}

template <typename AddressT, Repetition repetition, typename ContextT>
void repeat(
	AddressT &eCX,
	ContextT &context
) {
	if(
		repetition == Repetition::None ||		// No repetition => stop.
		!(--eCX)								// [e]cx is zero after being decremented => stop.
	) {
		return;
	}
	if constexpr (repetition != Repetition::Rep) {
		// If this is RepE or RepNE, also test the zero flag.
		if((repetition == Repetition::RepNE) == context.flags.template flag<Flag::Zero>()) {
			return;
		}
	}
	context.flow_controller.repeat_last();
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void cmps(
	const InstructionT &instruction,
	AddressT &eCX,
	AddressT &eSI,
	AddressT &eDI,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	IntT lhs = context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);
	const IntT rhs = context.memory.template access<IntT, AccessType::Read>(Source::ES, eDI);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	Primitive::sub<false, AccessType::Read, IntT>(lhs, rhs, context);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void scas(
	AddressT &eCX,
	AddressT &eDI,
	IntT &eAX,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	const IntT rhs = context.memory.template access<IntT, AccessType::Read>(Source::ES, eDI);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	Primitive::sub<false, AccessType::Read, IntT>(eAX, rhs, context);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void lods(
	const InstructionT &instruction,
	AddressT &eCX,
	AddressT &eSI,
	IntT &eAX,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	eAX = context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void movs(
	const InstructionT &instruction,
	AddressT &eCX,
	AddressT &eSI,
	AddressT &eDI,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) =
		context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);

	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void stos(
	AddressT &eCX,
	AddressT &eDI,
	const IntT eAX,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	if constexpr (uses_8086_exceptions(ContextT::model)) {
		try {
			context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) = eAX;
		} catch (const Exception &e) {
			// Empirical quirk of at least the 286: DI is adjusted even if the following access throws,
			// and CX has been adjusted... twice?
			//
			// (yes: including even if CX has already hit zero)
			if constexpr (ContextT::model <= Model::i80286) {
				eDI += context.flags.template direction<AddressT>() * sizeof(IntT);
				repeat<AddressT, repetition>(eCX, context);
				repeat<AddressT, repetition>(eCX, context);
			}

			throw e;
		}
	} else {
		context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) = eAX;
	}
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);
	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void outs(
	const InstructionT &instruction,
	AddressT &eCX,
	uint16_t port,
	AddressT &eSI,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.io.template out<IntT>(
		port,
		context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI)
	);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void ins(
	AddressT &eCX,
	uint16_t port,
	AddressT &eDI,
	ContextT &context
) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) = context.io.template in<IntT>(port);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

}
