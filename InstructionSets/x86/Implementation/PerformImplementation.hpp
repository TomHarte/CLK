//
//  PerformImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PerformImplementation_h
#define PerformImplementation_h

namespace InstructionSet::x86 {

namespace Primitive {

void aaa(CPU::RegisterPair16 &ax, Status &status) {
	/*
		IF ((AL AND 0FH) > 9) OR (AF = 1)
			THEN
				AL ← (AL + 6);
				AH ← AH + 1;
				AF ← 1;
				CF ← 1;
			ELSE
				AF ← 0;
				CF ← 0;
			FI;
		AL ← AL AND 0FH;
	*/
	/*
		The AF and CF flags are set to 1 if the adjustment results in a decimal carry;
		otherwise they are cleared to 0. The OF, SF, ZF, and PF flags are undefined.
	*/
	if((ax.halves.low & 0x0f) > 9 || status.auxiliary_carry) {
		ax.halves.low += 6;
		++ax.halves.high;
		status.auxiliary_carry = status.carry = 1;
	} else {
		status.auxiliary_carry = status.carry = 0;
	}
}

void aad(CPU::RegisterPair16 &ax, uint8_t imm, Status &status) {
	/*
		tempAL ← AL;
		tempAH ← AH;
		AL ← (tempAL + (tempAH * imm8)) AND FFH; (* imm8 is set to 0AH for the AAD mnemonic *)
		AH ← 0
	*/
	/*
		The SF, ZF, and PF flags are set according to the result;
		the OF, AF, and CF flags are undefined.
	*/
	ax.halves.low = ax.halves.low + (ax.halves.high * imm);
	ax.halves.high = 0;
	status.sign = ax.halves.low & 0x80;
	status.parity = status.zero = ax.halves.low;
}

}

template <
	Model model,
	Operation operation,
	DataSize data_size,
	typename FlowControllerT
> void perform(
	CPU::RegisterPair16 &destination,
	CPU::RegisterPair16 &source,
	Status &status,
	FlowControllerT &flow_controller
) {
	switch(operation) {
		case Operation::AAA:	Primitive::aaa(destination, status);					break;
		case Operation::AAD:	Primitive::aad(destination, source.halves.low, status);	break;
	}

	(void)flow_controller;
}


/*template <
	Model model,
	typename InstructionT,
	typename FlowControllerT,
	typename DataPointerResolverT,
	typename RegistersT,
	typename MemoryT,
	typename IOT,
	Operation operation
> void perform(
	const InstructionT &instruction,
	Status &status,
	FlowControllerT &flow_controller,
	DataPointerResolverT &resolver,
	RegistersT &registers,
	MemoryT &memory,
	IOT &io
) {
	switch((operation != Operation::Invalid) ? operation : instruction.operation) {
		default:
			assert(false);
			return;
	}
}*/


}

#endif /* PerformImplementation_h */
