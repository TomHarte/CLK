//
//  Z80AllRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80AllRAM_hpp
#define Z80AllRAM_hpp

#include "Z80.hpp"
#include "../AllRAMProcessor.hpp"

namespace CPU {
namespace Z80 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor,
	public Processor<AllRAMProcessor> {

	public:
		AllRAMProcessor();

		int perform_machine_cycle(const MachineCycle *cycle);
};

}
}

#endif /* Z80AllRAM_hpp */
