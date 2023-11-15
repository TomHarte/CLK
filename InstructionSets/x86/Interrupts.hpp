//
//  Interrupts.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Interrupts_h
#define InstructionSets_x86_Interrupts_h

namespace InstructionSet::x86 {

enum Interrupt {
	DivideError					= 0,
	SingleStep					= 1,
	NMI							= 2,
	Breakpoint					= 3,
	Overflow					= 4,
	BoundRangeExceeded			= 5,
	InvalidOpcode				= 6,
	DeviceNotAvailable			= 7,
	DoubleFault					= 8,
	CoprocessorSegmentOverrun	= 9,
	InvalidTSS					= 10,
	SegmentNotPresent			= 11,
	StackSegmentFault			= 12,
	GeneralProtectionFault		= 13,
	PageFault					= 14,
	/* 15 is reserved */
	FloatingPointException		= 16,
	AlignmentCheck				= 17,
	MachineCheck				= 18,

};

}

#endif /* InstructionSets_x86_Interrupts_h */
