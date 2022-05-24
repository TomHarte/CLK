//
//  ExceptionVectors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_ExceptionVectors_hpp
#define InstructionSets_M68k_ExceptionVectors_hpp

namespace InstructionSet {
namespace M68k {

enum Exception {
	InitialStackPointer					= 0,
	InitialProgramCounter				= 1,
	AccessFault							= 2,
	AddressError						= 3,
	IllegalInstruction					= 4,
	IntegerDivideByZero					= 5,
	CHK									= 6,
	TRAPV								= 7,
	PrivilegeViolation					= 8,
	Trace								= 9,
	Line1010							= 10,
	Line1111							= 11,
	CoprocessorProtocolViolation		= 13,
	FormatError							= 14,
	UninitialisedInterrupt				= 15,
	SpuriousInterrupt					= 24,
	InterruptAutovectorBase				= 25,	// This is the vector for interrupt level _1_.
	TrapBase							= 32,
	FPBranchOrSetOnUnorderedCondition	= 48,
	FPInexactResult						= 49,
	FPDivideByZero						= 50,
	FPUnderflow							= 51,
	FPOperandError						= 52,
	FPOverflow							= 53,
	FPSignallingNAN						= 54,
	FPUnimplementedDataType				= 55,
	MMUConfigurationError				= 56,
	MMUIllegalOperationError			= 57,
	MMUAccessLevelViolationError		= 58,
};

}
}

#endif /* InstructionSets_M68k_ExceptionVectors_hpp */
