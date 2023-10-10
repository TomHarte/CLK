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
	DivideError		= 0,
	SingleStep		= 1,
	NMI				= 2,
	OneByte			= 3,
	OnOverflow		= 4,
};

}

#endif /* InstructionSets_x86_Interrupts_h */
